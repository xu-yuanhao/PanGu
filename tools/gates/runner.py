"""Run controller-owned gate self-tests and bind their results to design inputs."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from receipt import canonical, capture, sha


SUITES = ("test_check.py", "test_module_graph.py", "test_compile_inputs.py", "test_link_graph.py",
          "test_service_graph.py", "test_receipt.py", "test_toolchain.py", "test_runner.py", "test_vcs.py",
          "test_native_build.py", "test_toolchain_probe.py", "test_remote.py", "test_controller_probe.py", "test_toolchain_image.py",
          "test_admission.py", "test_native_cpp.py", "test_native_ubsan.py", "test_admission_ubsan.py", "test_coordinator_trace.py")
CHILD = """
import json, pathlib, sys, unittest
directory = pathlib.Path(sys.argv[1])
sys.path.insert(0, str(directory))
suite = unittest.defaultTestLoader.discover(str(directory), pattern=sys.argv[2])
result = unittest.TextTestRunner(stream=sys.stderr, verbosity=1).run(suite)
summary = dict(tests=result.testsRun, failures=len(result.failures), errors=len(result.errors),
               skipped=len(result.skipped), expected_failures=len(result.expectedFailures),
               unexpected_successes=len(result.unexpectedSuccesses))
print(json.dumps(summary))
sys.exit(0 if result.wasSuccessful() and result.testsRun > 0 and not result.skipped and not result.expectedFailures else 1)
"""


def run(candidate, trusted, policy_pin):
    trusted = Path(trusted).resolve(strict=True)
    before = capture(candidate, trusted, policy_pin)
    registered = {Path(name).name for name in before["trusted_files"]
                  if Path(name).parent.as_posix() == "tools/gates"
                  and Path(name).name.startswith("test_") and name.endswith(".py")}
    if len(SUITES) != len(set(SUITES)) or registered != set(SUITES):
        raise ValueError("controller suite inventory missing, unscheduled or duplicated")
    results = []
    for suite in SUITES:
        name = "tools/gates/" + suite
        if name not in before["trusted_files"]:
            raise ValueError("required suite missing from controller: " + suite)
        command = [sys.executable, "-I", "-B", "-c", CHILD, str(trusted / "tools/gates"), suite]
        result = subprocess.run(command, cwd=trusted, env={"PATH": "/usr/bin:/bin", "LC_ALL": "C"},
                                capture_output=True, text=True, timeout=30)
        if result.returncode:
            raise ValueError("controller suite failed: " + suite + "\n" + result.stderr)
        try:
            summary = json.loads(result.stdout)
        except ValueError as error:
            raise ValueError("invalid test result: " + suite) from error
        required = {"tests", "failures", "errors", "skipped", "expected_failures", "unexpected_successes"}
        if (not isinstance(summary, dict) or set(summary) != required
                or any(type(v) is not int or v < 0 for v in summary.values())
                or summary["tests"] == 0 or any(summary[k] for k in required - {"tests"})):
            raise ValueError("incomplete or failed test coverage: " + suite)
        results.append({"suite": name, "command": command, "summary": summary,
                        "stdout": result.stdout, "stderr": result.stderr, "returncode": result.returncode})
    after = capture(candidate, trusted, policy_pin)
    if after != before:
        raise ValueError("inputs changed while controller tests ran")
    return {"schema": "pangu.controller-run.v1", "status": "BLOCKED", "input_receipt": before,
            "controller_self_tests": results, "test_count": sum(r["summary"]["tests"] for r in results),
            "candidate_native_validation": "NOT_RUN", "protected_executor": "NOT_ESTABLISHED",
            "release": "BLOCKED"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--trusted-root", type=Path, required=True)
    parser.add_argument("--policy-sha256", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        trusted = args.trusted_root.resolve(strict=True)
        if trusted != Path(__file__).resolve().parents[2]:
            raise ValueError("execute runner.py from the designated controller checkout")
        for root in (trusted, args.candidate.resolve(strict=True)):
            if args.output.resolve().is_relative_to(root):
                raise ValueError("run receipt must be outside input trees")
        if args.output.exists() or args.output.is_symlink():
            raise ValueError("run receipt already exists")
        report = run(args.candidate, trusted, args.policy_sha256)
        data = canonical(report) + b"\n"
        with args.output.open("xb") as output:
            output.write(data)
        print(json.dumps({"status": "BLOCKED", "controller_tests": report["test_count"],
                          "receipt": str(args.output.absolute()), "receipt_sha256": sha(data)}))
    except (ValueError, OSError, TypeError, KeyError, subprocess.SubprocessError) as error:
        print("controller run FAIL:", error)
        return 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
