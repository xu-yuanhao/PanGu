import json
from pathlib import Path
import shutil
import tempfile
import unittest

from receipt import canonical, capture, sha, verify


class ReceiptTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.trusted = Path(self.temp.name) / "trusted"
        self.candidate = Path(self.temp.name) / "candidate"
        self.trusted.mkdir()
        (self.trusted / "governance").mkdir()
        (self.trusted / "tools").mkdir()
        policy = {"schema_version": 1, "phase": "design-only", "excluded_roots": [".git", "references"],
                  "files": {"README.md": "document", "tools/check.py": "gate",
                            "governance/bootstrap-policy.json": "control"}}
        (self.trusted / "README.md").write_text("initial\n")
        (self.trusted / "tools/check.py").write_text("raise RuntimeError('must never execute candidate code')\n")
        path = self.trusted / "governance/bootstrap-policy.json"
        path.write_text(json.dumps(policy))
        self.policy_pin = sha(path.read_bytes())
        shutil.copytree(self.trusted, self.candidate)

    def capture(self):
        return capture(self.candidate, self.trusted, self.policy_pin)

    def saved(self):
        data = canonical(self.capture())
        return data, sha(data)

    def test_binding_without_executing_candidate(self):
        data, pin = self.saved()
        result = verify(self.candidate, self.trusted, self.policy_pin, data, pin)
        self.assertEqual(result, {"receipt_binding": "PASS", "status": "BLOCKED", "release": "BLOCKED"})

    def test_document_change_stales_receipt(self):
        data, pin = self.saved()
        (self.candidate / "README.md").write_text("changed\n")
        with self.assertRaisesRegex(ValueError, "stale"):
            verify(self.candidate, self.trusted, self.policy_pin, data, pin)

    def test_candidate_checker_change(self):
        (self.candidate / "tools/check.py").write_text("print('PASS')\n")
        with self.assertRaisesRegex(ValueError, "control differs"):
            self.capture()

    def test_deleted_checker(self):
        data, pin = self.saved()
        (self.candidate / "tools/check.py").unlink()
        with self.assertRaisesRegex(ValueError, "missing"):
            verify(self.candidate, self.trusted, self.policy_pin, data, pin)

    def test_candidate_policy_cannot_remove_checker(self):
        path = self.candidate / "governance/bootstrap-policy.json"
        policy = json.loads(path.read_text())
        del policy["files"]["tools/check.py"]
        path.write_text(json.dumps(policy))
        with self.assertRaisesRegex(ValueError, "control differs"):
            self.capture()

    def test_changed_trusted_policy_requires_new_pin(self):
        (self.trusted / "governance/bootstrap-policy.json").write_text("{}")
        with self.assertRaisesRegex(ValueError, "policy pin mismatch"):
            self.capture()

    def test_changed_trusted_checker_invalidates_receipt(self):
        data, pin = self.saved()
        for root in (self.trusted, self.candidate):
            (root / "tools/check.py").write_text("print('new controller')\n")
        with self.assertRaisesRegex(ValueError, "stale"):
            verify(self.candidate, self.trusted, self.policy_pin, data, pin)

    def test_forged_receipt_rejected_by_external_pin(self):
        data, pin = self.saved()
        report = json.loads(data)
        report["status"] = "PASS"
        with self.assertRaisesRegex(ValueError, "receipt pin mismatch"):
            verify(self.candidate, self.trusted, self.policy_pin, canonical(report), pin)

    def test_self_asserted_release_rejected_even_with_digest(self):
        data, _ = self.saved()
        report = json.loads(data)
        report["checks"]["release"] = "PASS"
        forged = canonical(report)
        with self.assertRaisesRegex(ValueError, "stale or contents invalid"):
            verify(self.candidate, self.trusted, self.policy_pin, forged, sha(forged))

    def test_unknown_source_rejected(self):
        (self.candidate / "extra.c").write_text("int hidden;\n")
        with self.assertRaisesRegex(ValueError, "unregistered file"):
            self.capture()

    def test_mode_change_stales_receipt(self):
        data, pin = self.saved()
        path = self.candidate / "README.md"
        path.chmod(path.stat().st_mode ^ 0o100)
        with self.assertRaisesRegex(ValueError, "stale"):
            verify(self.candidate, self.trusted, self.policy_pin, data, pin)

    def test_symlink_rejected(self):
        path = self.candidate / "README.md"
        path.unlink()
        path.symlink_to(self.trusted / "README.md")
        with self.assertRaisesRegex(ValueError, "non-regular"):
            self.capture()


if __name__ == "__main__":
    unittest.main()
