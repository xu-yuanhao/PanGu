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

    def test_native_source_scope(self):
        (self.root / "src/runtime").mkdir(parents=True)
        (self.root / "src/runtime/agent.c").write_text("int example;\n")
        self.policy["files"]["src/runtime/agent.c"] = "native-source"
        (self.root / "src/composition").mkdir()
        (self.root / "src/composition/pangu.c").write_text("int composition;\n")
        self.policy["files"]["src/composition/pangu.c"] = "native-source"
        self.assertEqual(inspect(self.root, self.policy), [])
        self.policy["files"]["outside.c"] = "native-source"
        self.assertTrue(any("invalid source scope" in e for e in inspect(self.root, self.policy)))

    def test_native_source_rejects_script(self):
        self.policy["files"]["src/runtime/hidden.py"] = "native-source"
        self.assertTrue(any("invalid suffix" in e for e in inspect(self.root, self.policy)))

    def test_legacy_and_client_scopes(self):
        for name, kind in [("src/arena/arena.S", "legacy-source"), ("src/client/tui/view.c", "client-source"),
                           ("tests/client/tui/check.h", "client-test"), ("tools/client/tui/build.sh", "client-build"),
                           (".gitignore", "repository-config")]:
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text("fixture\n")
            self.policy["files"][name]=kind
        self.assertEqual(inspect(self.root,self.policy),[])
        for name,kind in [("src/runtime/hidden.c","legacy-source"),("src/runtime/view.c","client-source"),
                          ("other-config","repository-config"),("tests/runtime/hidden.h","client-test")]:
            changed=copy.deepcopy(self.policy);changed["files"][name]=kind
            self.assertTrue(inspect(self.root,changed), (name,kind))

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
