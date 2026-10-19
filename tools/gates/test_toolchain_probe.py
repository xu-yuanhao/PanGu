import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from receipt import canonical
from toolchain_probe import experiment


class ToolchainExperimentTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.profile = json.loads((Path(__file__).resolve().parents[2] / "governance/native-probe-toolchain.json").read_text())
        self.pin = hashlib.sha256(canonical(self.profile)).hexdigest()

    def test_actual_repeatability_debugger_and_pic_experiment(self):
        summary = experiment(self.profile, self.pin, self.base / "experiment")
        self.assertEqual(summary["same_mode_elf_repeatability"], "PASS")
        self.assertEqual(summary["debug_lines"], "PASS")
        self.assertEqual(summary["assembly_debug_lines"], "PASS")
        self.assertEqual(summary["assembly_c_backtrace"], "PASS")
        self.assertEqual(summary["pic_shared_link"], "PASS")
        self.assertEqual(summary["fixture_scope"], "test-only")
        self.assertEqual(summary["status"], "BLOCKED")
        for label in ("debug1", "debug2", "release1", "release2"):
            self.assertEqual(summary["native_runs"][label]["exit_code"], 7)
            self.assertTrue((self.base / "experiment" / label / "probe.elf").is_file())

    def test_existing_evidence_not_overwritten(self):
        (self.base / "experiment").mkdir()
        with self.assertRaises(FileExistsError):
            experiment(self.profile, self.pin, self.base / "experiment")

    def test_profile_pin_is_not_optional(self):
        with self.assertRaisesRegex(ValueError, "toolchain pin mismatch"):
            experiment(self.profile, "0" * 64, self.base / "bad")
        self.assertFalse((self.base / "bad/experiment.json").exists())


if __name__ == "__main__":
    unittest.main()
