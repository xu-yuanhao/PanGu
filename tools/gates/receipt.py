"""Bind a design-phase inventory receipt to pinned policy and file contents.

Digests detect stale evidence; authority must come from an external protected pin.
No native build or release approval is issued by this tool.
"""
import argparse
import hashlib
import json
from pathlib import Path
import stat
import sys

from check import POLICY, inspect


def sha(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode()


def policy_at(trusted, expected):
    path = trusted / POLICY
    if path.is_symlink() or sha(path.read_bytes()) != expected:
        raise ValueError("trusted policy pin mismatch")
    policy = json.loads(path.read_text())
    errors = inspect(trusted, policy)
    if errors:
        raise ValueError("trusted tree invalid: " + "; ".join(errors))
    return policy


def snapshot(root, policy):
    errors = inspect(root, policy)
    if errors:
        raise ValueError("candidate inventory invalid: " + "; ".join(errors))
    return {name: {"sha256": sha((root / name).read_bytes()),
                   "mode": stat.S_IMODE((root / name).stat().st_mode)}
            for name in sorted(policy["files"])}


def capture(candidate, trusted, policy_pin):
    candidate, trusted = Path(candidate).resolve(strict=True), Path(trusted).resolve(strict=True)
    policy = policy_at(trusted, policy_pin)
    control = snapshot(trusted, policy)
    before = snapshot(candidate, policy)
    for name, kind in policy["files"].items():
        if kind in {"control", "gate", "gate-test", "instructions"} and before[name] != control[name]:
            raise ValueError("candidate control differs from trusted version: " + name)
    interpreter = Path(sys.executable).resolve(strict=True)
    python_sha = sha(interpreter.read_bytes())
    # Never run a candidate script. This receipt covers G00 and binding only.
    if snapshot(candidate, policy) != before or snapshot(trusted, policy) != control:
        raise ValueError("tree changed during capture")
    return {"schema": "pangu.design-receipt.v1", "status": "BLOCKED",
            "policy_sha256": policy_pin, "files": before, "trusted_files": control,
            "tree_sha256": sha(canonical(before)), "python": {"path": str(interpreter), "sha256": python_sha},
            "checks": {"G00": "PASS", "input_binding": "PASS", "native_build": "NOT_RUN",
                       "protected_executor": "NOT_ESTABLISHED", "release": "BLOCKED"}}


def verify(candidate, trusted, policy_pin, receipt_bytes, receipt_pin):
    if sha(receipt_bytes) != receipt_pin:
        raise ValueError("receipt pin mismatch")
    previous = json.loads(receipt_bytes)
    current = capture(candidate, trusted, policy_pin)
    if previous != current:
        raise ValueError("receipt stale or contents invalid")
    return {"receipt_binding": "PASS", "status": "BLOCKED", "release": "BLOCKED"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["capture", "verify"])
    parser.add_argument("--candidate", required=True, type=Path)
    parser.add_argument("--trusted-root", required=True, type=Path)
    parser.add_argument("--policy-sha256", required=True)
    parser.add_argument("--receipt", required=True, type=Path)
    parser.add_argument("--receipt-sha256")
    args = parser.parse_args()
    try:
        trusted = args.trusted_root.resolve(strict=True)
        # CLI must be launched from the designated controller checkout.
        if trusted != Path(__file__).resolve().parents[2]:
            raise ValueError("execute receipt.py from the trusted controller checkout")
        path = args.receipt.absolute()
        for root in (trusted, args.candidate.resolve(strict=True)):
            if path.resolve().is_relative_to(root):
                raise ValueError("receipt must be outside input trees")
        if args.mode == "capture":
            report = capture(args.candidate, trusted, args.policy_sha256)
            data = canonical(report) + b"\n"
            # Never overwrite earlier evidence.
            with path.open("xb") as output:
                output.write(data)
            print(json.dumps({"status": "BLOCKED", "receipt_sha256": sha(data), "receipt": str(path)}))
        else:
            if not args.receipt_sha256:
                raise ValueError("external receipt pin required")
            print(json.dumps(verify(args.candidate, trusted, args.policy_sha256,
                                    path.read_bytes(), args.receipt_sha256)))
    except (OSError, ValueError, TypeError, KeyError) as error:
        print("receipt FAIL:", error)
        return 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
