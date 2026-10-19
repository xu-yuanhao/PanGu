import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import runner
from receipt import sha


class ControllerRunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "governance").mkdir()
        (self.root / "tools/gates").mkdir(parents=True)
        files = {"governance/bootstrap-policy.json": "control", "README.md": "document"}
        for suite in runner.SUITES:
            name = "tools/gates/" + suite
            files[name] = "gate-test"
            (self.root / name).write_text("raise RuntimeError('mocked controller fixture')\n")
        (self.root / "README.md").write_text("design\n")
        policy = {"schema_version": 1, "phase": "design-only", "excluded_roots": [".git", "references"], "files": files}
        path = self.root / "governance/bootstrap-policy.json"
        path.write_text(json.dumps(policy))
        self.pin = sha(path.read_bytes())
        self.summary = {"tests": 1, "failures": 0, "errors": 0, "skipped": 0,
                        "expected_failures": 0, "unexpected_successes": 0}

    def result(self):
        return subprocess.CompletedProcess([], 0, json.dumps(self.summary), "test log\n")

    def test_controller_commands_and_bound_results(self):
        with patch.object(runner.subprocess, "run", return_value=self.result()) as process:
            report = runner.run(self.root, self.root, self.pin)
        self.assertEqual(process.call_count, len(runner.SUITES))
        self.assertEqual(report["test_count"], len(runner.SUITES))
        self.assertEqual(report["status"], "BLOCKED")
        self.assertEqual(report["candidate_native_validation"], "NOT_RUN")
        for call in process.call_args_list:
            self.assertIn("-I", call.args[0])
            self.assertEqual(call.kwargs["cwd"], self.root)
            self.assertEqual(call.args[0][-2], str(self.root / "tools/gates"))

    def test_missing_suite_rejected(self):
        (self.root / "tools/gates" / runner.SUITES[-1]).unlink()
        with patch.object(runner.subprocess, "run") as process, self.assertRaisesRegex(ValueError, "missing"):
            runner.run(self.root, self.root, self.pin)
        process.assert_not_called()

    def test_zero_tests_and_nonpass_rejected(self):
        for key in ("tests", "failures", "errors", "skipped", "expected_failures", "unexpected_successes"):
            summary = dict(self.summary)
            summary[key] = 0 if key == "tests" else 1
            result = subprocess.CompletedProcess([], 0, json.dumps(summary), "")
            with self.subTest(key=key), patch.object(runner.subprocess, "run", return_value=result):
                with self.assertRaisesRegex(ValueError, "incomplete or failed"):
                    runner.run(self.root, self.root, self.pin)

    def test_crash_rejected(self):
        result = subprocess.CompletedProcess([], -11, "", "crash")
        with patch.object(runner.subprocess, "run", return_value=result), self.assertRaisesRegex(ValueError, "suite failed"):
            runner.run(self.root, self.root, self.pin)

    def test_timeout_is_not_success(self):
        with patch.object(runner.subprocess, "run", side_effect=subprocess.TimeoutExpired("test", 30)):
            with self.assertRaises(subprocess.TimeoutExpired):
                runner.run(self.root, self.root, self.pin)

    def test_bad_output_rejected(self):
        result = subprocess.CompletedProcess([], 0, "PASS", "")
        with patch.object(runner.subprocess, "run", return_value=result), self.assertRaisesRegex(ValueError, "invalid test result"):
            runner.run(self.root, self.root, self.pin)

    def test_tree_change_during_execution(self):
        def mutate(*args, **kwargs):
            (self.root / "README.md").write_text("changed\n")
            return self.result()
        with patch.object(runner.subprocess, "run", side_effect=mutate), self.assertRaisesRegex(ValueError, "inputs changed"):
            runner.run(self.root, self.root, self.pin)


if __name__ == "__main__":
    unittest.main()
