"""Local Git boundary audit; no remote protection or commit approval implied."""
import argparse
import json
from pathlib import Path
import subprocess

from check import POLICY, inspect


def git(root, *arguments, allow_failure=False):
    result = subprocess.run(["/usr/bin/git", "-c", "core.hooksPath=/dev/null", "-C", str(root), *arguments],
                            env={"PATH": "/usr/bin:/bin", "LC_ALL": "C", "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": "/dev/null"},
                            capture_output=True, timeout=15)
    if result.returncode and not allow_failure:
        raise ValueError("Git inspection failed: " + result.stderr.decode(errors="replace"))
    return result


def audit(root):
    root = Path(root).resolve(strict=True)
    if not (root / ".git").is_dir() or (root / ".git").is_symlink():
        raise ValueError("expected local standalone Git repository")
    toplevel = Path(git(root, "rev-parse", "--show-toplevel").stdout.decode().strip()).resolve()
    if toplevel != root:
        raise ValueError("Git repository root mismatch")
    policy_path = root / POLICY
    if policy_path.is_symlink():
        raise ValueError("policy symlink forbidden")
    policy = json.loads(policy_path.read_text())
    errors = inspect(root, policy)
    if errors:
        raise ValueError("inventory failed: " + "; ".join(errors))
    indexed = set()
    for record in git(root, "ls-files", "--stage", "-z").stdout.split(b"\0"):
        if not record:
            continue
        metadata, raw_path = record.split(b"\t", 1)
        mode, oid, stage = metadata.decode().split()
        path = raw_path.decode("utf-8")
        if path not in policy["files"]:
            raise ValueError("indexed file outside approved inventory: " + path)
        if mode not in {"100644", "100755"} or stage != "0":
            raise ValueError("unsupported or unresolved Git index entry: " + path)
        indexed.add(path)
    # Check the exclusion itself, rather than relying only on today's empty index.
    result = git(root, "check-ignore", "--no-index", "references/__pangu_boundary_probe__", allow_failure=True)
    if result.returncode != 0:
        raise ValueError("references exclusion missing")
    head = git(root, "rev-parse", "--verify", "HEAD", allow_failure=True)
    return {"status": "BLOCKED", "local_git_boundary": "PASS", "indexed_files": len(indexed),
            "unindexed_registered_files": sorted(set(policy["files"]) - indexed),
            "head": head.stdout.decode().strip() if head.returncode == 0 else "NOT_CREATED",
            "remote_protection": "NOT_VERIFIED", "release": "BLOCKED"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.root), ensure_ascii=False, indent=2))
    except (ValueError, OSError, TypeError, subprocess.SubprocessError) as error:
        print("Git boundary FAIL:", error)
        return 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
