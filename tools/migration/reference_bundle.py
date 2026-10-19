"""Freeze pinned Git references and verify offline recovery without executing them.

Bundles contain source history, not installed dependencies or a runnable oracle.
Outputs must live outside the product checkout. No network access is required.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat
import subprocess
import tempfile


SCHEMA = "pangu.reference-bundle.v1"


def git(repo, *args, input_data=None):
    result = subprocess.run(
        ["git", "-c", "core.hooksPath=/dev/null", "-C", str(repo), *args],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
        input=input_data,
        env={**os.environ, "GIT_OPTIONAL_LOCKS": "0", "GIT_TERMINAL_PROMPT": "0",
             "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": os.devnull},
    )
    if result.returncode:
        raise ValueError(f"git {args[0]} failed: {result.stderr.decode(errors='replace').strip()}")
    return result.stdout


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def canonical_digest(value):
    raw = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True)
    return hashlib.sha256(raw.encode()).hexdigest()


def safe_name(name):
    p = PurePosixPath(name)
    if (not name or p.is_absolute() or ".." in p.parts
            or str(p) != name or "\\" in name or "\x00" in name):
        raise ValueError(f"unsafe relative path: {name!r}")
    return p


def tree(repo, commit):
    entries = []
    for record in git(repo, "ls-tree", "-rz", "--full-tree", commit).split(b"\0"):
        if not record:
            continue
        meta, raw_path = record.split(b"\t", 1)
        mode, kind, oid = meta.decode("ascii").split()
        path = os.fsdecode(raw_path)
        safe_name(path)
        if kind != "blob" or mode not in {"100644", "100755", "120000"}:
            raise ValueError(f"unsupported tree entry (submodules need separate pins): {path}")
        entries.append({"path": path, "mode": mode, "git_oid": oid})
    return entries


def snapshot(repo, commit):
    """Compare every worktree byte/mode with its pinned Git blob, including links."""
    if git(repo, "rev-parse", "HEAD").decode().strip() != commit:
        raise ValueError(f"reference HEAD changed: {repo.name}")
    status = git(repo, "status", "--porcelain=v1", "--untracked-files=all")
    if status:
        raise ValueError(f"reference has tracked/untracked changes: {repo.name}")
    ignored = git(repo, "ls-files", "--others", "--ignored", "--exclude-standard", "-z")
    if ignored:
        raise ValueError(f"ignored files not admitted as reference inputs: {repo.name}")
    algorithm = git(repo, "rev-parse", "--show-object-format").decode().strip()
    entries = tree(repo, commit)
    attr_output = git(repo, "check-attr", "--cached", "-z", "--stdin",
                      "text", "eol", "filter", "working-tree-encoding",
                      input_data=b"".join(os.fsencode(e["path"]) + b"\0" for e in entries))
    fields = attr_output.split(b"\0")[:-1]
    attributes = {}
    for i in range(0, len(fields), 3):
        path, name, value = fields[i:i + 3]
        attributes.setdefault(os.fsdecode(path), {})[name.decode()] = value.decode()
    files = []
    for entry in entries:
        path = repo / entry["path"]
        for parent in path.relative_to(repo).parents:
            if (repo / parent).is_symlink():
                raise ValueError(f"symlink parent in reference: {entry['path']}")
        metadata = path.lstat()
        if entry["mode"] == "120000":
            if not stat.S_ISLNK(metadata.st_mode):
                raise ValueError(f"expected symlink: {entry['path']}")
            raw = os.fsencode(os.readlink(path))
        else:
            if not stat.S_ISREG(metadata.st_mode):
                raise ValueError(f"expected regular file: {entry['path']}")
            executable = bool(metadata.st_mode & 0o111)
            if executable != (entry["mode"] == "100755"):
                raise ValueError(f"executable mode mismatch: {entry['path']}")
            raw = path.read_bytes()
        attrs = attributes[entry["path"]]
        # Git checks out Windows scripts as CRLF when attributes request it.
        # Freeze canonical source bytes, not platform-dependent checkout line endings.
        canonical = raw
        raw_oid = hashlib.new(algorithm, f"blob {len(raw)}\0".encode() + raw).hexdigest()
        if raw_oid != entry["git_oid"] and any(
                attrs[a] not in {"unspecified", "unset"}
                for a in ("filter", "working-tree-encoding")):
            raise ValueError(f"unsupported materialized checkout transform: {entry['path']}")
        if raw_oid != entry["git_oid"] and entry["mode"] != "120000" and (
                attrs["text"] == "set" or attrs["text"] == "auto" and b"\0" not in raw):
            canonical = raw.replace(b"\r\n", b"\n")
        blob = hashlib.new(algorithm, f"blob {len(canonical)}\0".encode() + canonical).hexdigest()
        if blob != entry["git_oid"]:
            raise ValueError(f"worktree bytes differ from pin: {entry['path']}")
        files.append({**entry, "sha256": hashlib.sha256(canonical).hexdigest(),
                      "size": len(canonical), "checkout_filter": attrs["filter"],
                      "working_tree_encoding": attrs["working-tree-encoding"]})
    return files


def verify(bundle_dir):
    """Restore every source into a fresh directory and recheck every tracked file."""
    manifest_path = bundle_dir / "manifest.json"
    if manifest_path.is_symlink():
        raise ValueError("manifest must not be a symlink")
    manifest = json.loads(manifest_path.read_text())
    if manifest.get("schema") != SCHEMA or not manifest.get("repositories"):
        raise ValueError("unsupported or empty reference manifest")
    seen = set()
    with tempfile.TemporaryDirectory(prefix="pangu-reference-restore-") as temporary:
        for item in manifest["repositories"]:
            ident = item["id"]
            safe_name(ident)
            if "/" in ident or ident in seen:
                raise ValueError("duplicate or nested reference ID")
            seen.add(ident)
            for key, hash_key in (("bundle", "bundle_sha256"), ("inventory", "inventory_sha256")):
                safe_name(item[key])
                path = bundle_dir / item[key]
                if path.is_symlink() or not path.is_file() or digest(path) != item[hash_key]:
                    raise ValueError(f"artifact hash/type mismatch: {item[key]}")
            inventory = json.loads((bundle_dir / item["inventory"]).read_text())
            if canonical_digest(inventory) != item["content_digest"]:
                raise ValueError(f"inventory content digest mismatch: {ident}")
            restored = Path(temporary) / ident
            # No source hooks are copied or run; no package scripts are executed.
            git(Path(temporary), "clone", "--quiet", "--no-checkout",
                str((bundle_dir / item["bundle"]).resolve()), str(restored))
            git(restored, "-c", "advice.detachedHead=false", "checkout", "--quiet",
                "--detach", item["commit"])
            actual = snapshot(restored, item["commit"])
            if actual != inventory or len(actual) != item["file_count"]:
                raise ValueError(f"restored source differs: {ident}")
    return manifest


def create(root, baseline_path, output):
    root, output = root.resolve(), output.resolve()
    if output.is_relative_to(root):
        raise ValueError("bundle outputs must be outside the product checkout")
    if output.exists():
        raise ValueError("output exists; refusing to overwrite a frozen reference bundle")
    baseline = json.loads(baseline_path.read_text())
    if baseline.get("schema") != "pangu.migration.reference-baseline.v1":
        raise ValueError("unsupported reference baseline")
    repos = baseline.get("repositories", [])
    if not repos:
        raise ValueError("empty reference baseline")
    seen = set()
    prepared = []
    for item in repos:
        safe_name(item["id"])
        safe_name(item["path"])
        if "/" in item["id"] or item["id"] in seen:
            raise ValueError("duplicate or nested reference ID")
        seen.add(item["id"])
        repo = root / item["path"]
        if repo.is_symlink() or not repo.resolve().is_relative_to(root):
            raise ValueError("reference path escapes checkout")
        origin = git(repo, "remote", "get-url", "origin").decode().strip()
        if origin != item["origin"]:
            raise ValueError(f"reference origin changed: {item['id']}")
        files = snapshot(repo, item["commit"])
        by_path = {f["path"]: f for f in files}
        for key in item["key_files"]:
            current = by_path.get(key["path"])
            if key["sha256"] is None:
                if current is not None:
                    raise ValueError("previously missing reference artifact appeared")
            elif current is None or current["sha256"] != key["sha256"]:
                raise ValueError(f"baseline key-file mismatch: {item['id']}/{key['path']}")
        prepared.append((item, repo, files))
    output.parent.mkdir(parents=True, exist_ok=True)
    # Publish only after recovery verification. Interrupted creation never claims success.
    with tempfile.TemporaryDirectory(prefix=".pangu-reference-stage-", dir=output.parent) as stage_name:
        stage = Path(stage_name)
        manifest = {"schema": SCHEMA, "baseline_sha256": digest(baseline_path),
                    "git_version": git(root, "--version").decode().strip(),
                    "dependencies_installed": False, "oracle_executed": False,
                    "repositories": []}
        for item, repo, files in prepared:
            ident = item["id"]
            bundle = stage / f"{ident}.bundle"
            inventory = stage / f"{ident}.files.json"
            git(repo, "bundle", "create", str(bundle), "HEAD")
            inventory.write_text(json.dumps(files, ensure_ascii=True, indent=2) + "\n")
            manifest["repositories"].append({
                "id": ident, "commit": item["commit"], "origin": item["origin"],
                "tree_oid": git(repo, "rev-parse", item["commit"] + "^{tree}").decode().strip(),
                "file_count": len(files), "content_digest": canonical_digest(files),
                "opaque_filtered_files": [f["path"] for f in files
                                          if f["checkout_filter"] not in {"unspecified", "unset"}],
                "materialization": "Git blobs only; filtered/LFS payloads are not fetched or oracle inputs",
                "bundle": bundle.name, "bundle_sha256": digest(bundle),
                "inventory": inventory.name, "inventory_sha256": digest(inventory),
                "lockfiles": [f["path"] for f in files if Path(f["path"]).name in {
                    "package-lock.json", "pnpm-lock.yaml", "yarn.lock", "Cargo.lock"}],
                "license_files": [f["path"] for f in files if Path(f["path"]).name.upper().startswith(
                    ("LICENSE", "COPYING", "LEGAL", "NOTICE", "THIRD_PARTY"))],
            })
        (stage / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=True, indent=2) + "\n")
        verify(stage)
        for item, repo, files in prepared:
            if snapshot(repo, item["commit"]) != files:
                raise ValueError("reference changed while creating bundle")
        stage.rename(output)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["create", "verify"])
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.action == "create":
            baseline = args.baseline or args.root / "docs/migration/reference-baseline.json"
            manifest = create(args.root, baseline, args.output)
        else:
            manifest = verify(args.output.resolve())
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"REFERENCE FAIL: {error}")
        return 1
    print(f"REFERENCE PASS: {len(manifest['repositories'])} repositories restored and verified")
    print("Oracle dependencies and execution: NOT RUN (T09)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
