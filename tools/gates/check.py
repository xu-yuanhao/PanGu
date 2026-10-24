"""G00: default-deny file admission for Pangu's design-only phase.

This is a local inventory check, not a trusted CI runner or production-code audit.
"""

import argparse
import json
import os
from pathlib import Path, PurePosixPath


POLICY = "governance/bootstrap-policy.json"
KINDS = {"instructions", "document", "control", "gate", "gate-test", "native-source", "native-test", "development-tool", "legacy-source", "client-source", "client-test", "client-build", "repository-config"}
# The T09 oracle runner must stay JavaScript: it imports the frozen Pi sources.
# Only "gate" may use .mjs; documents/controls/tests stay .md/.json/.py.
SUFFIXES = {"instructions": ".md", "document": ".md", "control": ".json",
            "gate": (".py", ".mjs"), "gate-test": ".py",
            "native-source": (".c", ".h"), "native-test": ".c",
            "development-tool": ".py", "legacy-source": (".c", ".h", ".cpp", ".S"),
            "client-source": (".c", ".h"), "client-test": (".c", ".h"),
            "client-build": ".sh", "repository-config": ""}


def inspect(root, policy):
    """Return all inventory violations; never treat invalid policy as success."""
    errors = []
    if not isinstance(policy, dict) or set(policy) != {
        "schema_version", "phase", "excluded_roots", "files"
    }:
        return ["policy: invalid fields"]
    if policy["schema_version"] != 1 or policy["phase"] != "design-only":
        return ["policy: unsupported version or phase"]
    if policy["excluded_roots"] != [".git", "references"]:
        return ["policy: unsupported exclusions"]
    files = policy["files"]
    if not isinstance(files, dict) or not files:
        return ["policy: files must be a nonempty mapping"]
    for name, kind in files.items():
        path = PurePosixPath(name)
        if (not name or path.is_absolute() or ".." in path.parts
                or str(path) != name or "\\" in name
                or path.parts[0] in policy["excluded_roots"]
                or kind not in KINDS):
            errors.append(f"policy: invalid file registration {name!r}")
            continue
        roots = {"native-source": ("src/runtime/", "src/contracts/"),
                 "native-test": ("tests/runtime/",),
                 "development-tool": ("tools/runtime/",),
                 "client-source": ("src/client/tui/",),
                 "client-test": ("tests/client/tui/",),
                 "client-build": ("tools/client/tui/",)}
        if kind in roots and not name.startswith(roots[kind]):
            errors.append(f"policy: invalid source scope for {kind}: {name}")
        if kind == "repository-config" and name != ".gitignore":
            errors.append(f"policy: invalid repository config: {name}")
        if kind == "legacy-source" and (not name.startswith("src/") or name.startswith(("src/runtime/", "src/contracts/", "src/client/"))):
            errors.append(f"policy: invalid legacy source scope: {name}")
        expected = SUFFIXES[kind]
        if path.suffix not in (expected if isinstance(expected, tuple) else (expected,)):
            errors.append(f"policy: invalid suffix for {kind}: {name}")
    if errors:
        return errors
    found = set()

    def onerror(error):
        errors.append(f"inventory: cannot read directory: {error}")

    for directory, dirs, names in os.walk(root, followlinks=False, onerror=onerror):
        parent = Path(directory)
        for name in list(dirs):
            item = parent / name
            rel = item.relative_to(root).as_posix()
            if rel in policy["excluded_roots"]:
                # Excluded trees are not trusted build inputs.
                dirs.remove(name)
            elif item.is_symlink():
                errors.append(f"inventory: symlink forbidden: {rel}")
                dirs.remove(name)
            elif not any(p.startswith(rel + "/") for p in files):
                errors.append(f"inventory: unregistered directory: {rel}")
        for name in names:
            item = parent / name
            rel = item.relative_to(root).as_posix()
            if rel in policy["excluded_roots"]:
                continue
            found.add(rel)
            if item.is_symlink() or not item.is_file():
                errors.append(f"inventory: non-regular file forbidden: {rel}")
            elif rel not in files:
                errors.append(f"inventory: unregistered file: {rel}")
    for name in sorted(set(files) - found):
        errors.append(f"inventory: registered file missing: {name}")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        policy_path = root / POLICY
        if policy_path.is_symlink():
            raise ValueError("policy must not be a symlink")
        policy = json.loads(policy_path.read_text(encoding="utf-8"))
        errors = inspect(root, policy)
    except (OSError, ValueError, TypeError) as error:
        print(f"G00 FAIL: {error}")
        return 1
    for error in errors:
        print(f"G00 FAIL: {error}")
    if errors:
        return 1
    print("G00 PASS: registered inventory admitted (includes scoped local native development)")
    print("Production architecture/build/ABI/performance admission: BLOCKED; candidate probes do not grant admission")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
