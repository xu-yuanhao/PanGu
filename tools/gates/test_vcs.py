import json
from pathlib import Path
import tempfile
import unittest

from vcs import audit, git


class GitBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        git(self.root, "init", "--initial-branch=main")
        (self.root / ".git/info/exclude").write_text("/references/\n")
        (self.root / "governance").mkdir()
        (self.root / "README.md").write_text("design\n")
        (self.root / "references").mkdir()
        (self.root / "references/upstream.c").write_text("int upstream;\n")
        self.policy = {"schema_version": 1, "phase": "design-only", "excluded_roots": [".git", "references"],
                       "files": {"README.md": "document", "governance/bootstrap-policy.json": "control"}}
        self.save()

    def save(self):
        (self.root / "governance/bootstrap-policy.json").write_text(json.dumps(self.policy))

    def test_initial_repository_not_release(self):
        report = audit(self.root)
        self.assertEqual(report["local_git_boundary"], "PASS")
        self.assertEqual(report["head"], "NOT_CREATED")
        self.assertEqual(report["status"], "BLOCKED")
        self.assertEqual(report["indexed_files"], 0)

    def test_registered_staging(self):
        git(self.root, "add", "README.md")
        self.assertEqual(audit(self.root)["indexed_files"], 1)

    def test_forced_reference_staging_rejected(self):
        git(self.root, "add", "-f", "references/upstream.c")
        with self.assertRaisesRegex(ValueError, "outside approved inventory"):
            audit(self.root)

    def test_removed_exclusion(self):
        (self.root / ".git/info/exclude").write_text("")
        with self.assertRaisesRegex(ValueError, "exclusion missing"):
            audit(self.root)

    def test_indexed_then_deleted_unknown_file(self):
        (self.root / "hidden.c").write_text("int hidden;\n")
        git(self.root, "add", "hidden.c")
        (self.root / "hidden.c").unlink()
        with self.assertRaisesRegex(ValueError, "outside approved inventory"):
            audit(self.root)

    def test_missing_repository(self):
        with tempfile.TemporaryDirectory() as empty:
            with self.assertRaisesRegex(ValueError, "standalone Git"):
                audit(empty)


if __name__ == "__main__":
    unittest.main()
