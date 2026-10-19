"""Positive and rejection tests for G00; fixtures never touch project files."""

import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

# Avoid creating unregistered __pycache__ files in the source tree.
sys.dont_write_bytecode = True
from check import inspect


class AdmissionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.policy = {
            "schema_version": 1,
            "phase": "design-only",
            "excluded_roots": [".git", "references"],
            "files": {"README.md": "document"},
        }
        (self.root / "README.md").write_text("design\n")

    def test_valid_inventory(self):
        self.assertEqual(inspect(self.root, self.policy), [])

    def test_unregistered_source_even_with_unknown_suffix(self):
        (self.root / "hidden.payload").write_text("code")
        self.assertTrue(any("unregistered file" in e
                            for e in inspect(self.root, self.policy)))

    def test_source_cannot_be_admitted_as_document(self):
        self.policy["files"]["runtime.c"] = "document"
        self.assertTrue(any("invalid suffix" in e
                            for e in inspect(self.root, self.policy)))

    def test_missing_registration_target(self):
        (self.root / "README.md").unlink()
        self.assertTrue(any("missing" in e for e in inspect(self.root, self.policy)))

    def test_symlink_file(self):
        (self.root / "README.md").unlink()
        (self.root / "README.md").symlink_to("/dev/null")
        self.assertTrue(any("non-regular" in e
                            for e in inspect(self.root, self.policy)))

    def test_symlink_directory(self):
        (self.root / "shortcut").symlink_to(self.root, target_is_directory=True)
        self.assertTrue(any("symlink" in e for e in inspect(self.root, self.policy)))

    def test_empty_unregistered_directory(self):
        (self.root / "runtime").mkdir()
        self.assertTrue(any("unregistered directory" in e
                            for e in inspect(self.root, self.policy)))

    def test_excluded_reference_tree(self):
        (self.root / "references").mkdir()
        (self.root / "references" / "upstream.c").write_text("reference")
        self.assertEqual(inspect(self.root, self.policy), [])

    def test_no_added_exclusion_or_production_phase(self):
        changed = copy.deepcopy(self.policy)
        changed["excluded_roots"].append("runtime")
        self.assertTrue(inspect(self.root, changed))
        changed = copy.deepcopy(self.policy)
        changed["phase"] = "production"
        self.assertTrue(inspect(self.root, changed))

    def test_oracle_runner_mjs_only_as_gate(self):
        self.policy["files"]["tools/oracle.mjs"] = "gate"
        (self.root / "tools").mkdir()
        (self.root / "tools" / "oracle.mjs").write_text("export {};\n")
        self.assertEqual(inspect(self.root, self.policy), [])

    def test_mjs_cannot_be_document_control_or_test(self):
        for kind in ("document", "control", "gate-test", "instructions"):
            changed = copy.deepcopy(self.policy)
            changed["files"]["tools/oracle.mjs"] = kind
            self.assertTrue(any("invalid suffix" in e for e in inspect(self.root, changed)), kind)

    def test_path_escape(self):
        self.policy["files"]["../escape.md"] = "document"
        self.assertTrue(inspect(self.root, self.policy))

    def test_invalid_policy_fails_cli(self):
        (self.root / "governance").mkdir()
        (self.root / "governance/bootstrap-policy.json").write_text("{broken")
        result = subprocess.run(
            [sys.executable, str(Path(__file__).with_name("check.py")),
             "--root", str(self.root)], capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("G00 FAIL", result.stdout)


if __name__ == "__main__":
    unittest.main()
