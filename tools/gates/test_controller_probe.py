from pathlib import Path
import tempfile
import unittest

from controller_probe import copy_registered
from receipt import snapshot


class ControllerCopyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "source"
        self.root.mkdir()
        (self.root / "README.md").write_text("design\n")
        self.policy = {"schema_version": 1, "phase": "design-only", "excluded_roots": [".git", "references"], "files": {"README.md": "document"}}

    def test_copies_content_and_mode(self):
        (self.root / "README.md").chmod(0o600)
        copy_registered(self.root, self.base / "controller", self.policy)
        self.assertEqual(snapshot(self.root, self.policy), snapshot(self.base / "controller", self.policy))

    def test_excluded_references_not_copied(self):
        (self.root / "references").mkdir()
        (self.root / "references/upstream.c").write_text("reference\n")
        copy_registered(self.root, self.base / "controller", self.policy)
        self.assertFalse((self.base / "controller/references").exists())

    def test_nested_copy_rejected(self):
        with self.assertRaisesRegex(ValueError, "outside source"):
            copy_registered(self.root, self.root / "controller", self.policy)

    def test_existing_evidence_not_overwritten(self):
        (self.base / "controller").mkdir()
        with self.assertRaises(FileExistsError):
            copy_registered(self.root, self.base / "controller", self.policy)


if __name__ == "__main__":
    unittest.main()
