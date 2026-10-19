"""Exercise separate controller/candidate trees and deliberate gate-bypass attempts."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

from receipt import POLICY, canonical, capture, policy_at, sha, snapshot


def copy_registered(source, destination, policy):
    source, destination = Path(source).resolve(strict=True), Path(destination).absolute()
    if destination.resolve().is_relative_to(source):
        raise ValueError("controller copy must be outside source")
    destination.mkdir()
    for name in policy["files"]:
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / name, path)
    if snapshot(destination, policy) != snapshot(source, policy):
        raise ValueError("controller copy does not match source")


def experiment(source, policy_pin, output):
    source, output = Path(source).resolve(strict=True), Path(output).absolute()
    if output.resolve().is_relative_to(source):
        raise ValueError("experiment output must be outside source")
    policy = policy_at(source, policy_pin)
    initial = capture(source, source, policy_pin)
    output.mkdir()
    controller, candidate = output / "controller", output / "candidate"
    copy_registered(source, controller, policy)
    copy_registered(source, candidate, policy)
    bootstrap = "import runpy,sys;sys.path.insert(0,sys.argv[1]);sys.argv=sys.argv[2:];runpy.run_path(sys.argv[0],run_name='__main__')"
    base = [sys.executable, "-I", "-B", "-c", bootstrap, str(controller / "tools/gates"), str(controller / "tools/gates/runner.py"), "--candidate", str(candidate),
            "--trusted-root", str(controller), "--policy-sha256", policy_pin]
    def invoke(name):
        receipt = output / (name + ".json")
        command = base + ["--output", str(receipt)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60,
                                env={"PATH": "/usr/bin:/bin", "LC_ALL": "C"})
        return result, receipt, command
    result, receipt, command = invoke("baseline")
    if result.returncode != 2 or not receipt.is_file():
        raise ValueError("separate controller run failed: " + result.stdout + result.stderr)
    baseline = {"command": command, "returncode": result.returncode, "stdout": result.stdout,
                "stderr": result.stderr, "receipt_sha256": sha(receipt.read_bytes())}
    attacks = []
    changes = {"delete-gate": ("tools/gates/check.py", None),
               "fake-pass-gate": ("tools/gates/check.py", b"print('G00 PASS: forged')\n"),
               "remove-policy-check": (POLICY, canonical({**policy, "files": {k: v for k, v in policy["files"].items() if k != "tools/gates/check.py"}})),
               "unknown-source": ("hidden.c", b"int hidden;\n"),
               "change-toolchain": ("governance/native-probe-toolchain.json", b"{}\n")}
    for name, (relative, content) in changes.items():
        path = candidate / relative
        original = path.read_bytes() if path.exists() else None
        if content is None:
            path.unlink()
        else:
            path.write_bytes(content)
        try:
            result, receipt, command = invoke(name)
            if result.returncode != 1 or receipt.exists():
                raise ValueError("bypass attempt was not rejected: " + name)
            attacks.append({"attack": name, "command": command, "returncode": result.returncode,
                            "stdout": result.stdout, "stderr": result.stderr, "receipt_created": False})
        finally:
            if original is None:
                path.unlink(missing_ok=True)
            else:
                path.write_bytes(original)
    if capture(source, source, policy_pin) != initial:
        raise ValueError("source changed during controller experiment")
    if capture(candidate, controller, policy_pin)["files"] != initial["files"]:
        raise ValueError("experiment did not restore candidate")
    summary = {"schema": "pangu.controller-experiment.v1", "status": "BLOCKED", "input_receipt": initial,
               "baseline": baseline, "bypass_rejections": attacks, "controller_and_candidate": "SEPARATE_DIRECTORIES",
               "permission_separation": "NOT_ESTABLISHED", "independent_approval": "PENDING", "release": "BLOCKED"}
    (output / "experiment.json").write_bytes(canonical(summary) + b"\n")
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--policy-sha256", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = experiment(args.source, args.policy_sha256, args.output)
    except (ValueError, OSError, TypeError, KeyError, subprocess.SubprocessError) as error:
        print("controller experiment FAIL:", error)
        return 1
    print(json.dumps({"status": report["status"], "bypass_rejections": len(report["bypass_rejections"]),
                      "experiment": str(args.output / "experiment.json"), "experiment_sha256": sha((args.output / "experiment.json").read_bytes())}))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
