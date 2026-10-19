"""Verify real offline recovery and fail-closed behavior using small Git fixtures."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
from reference_bundle import create, digest, git, snapshot, verify


class ReferenceBundleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pangu-ref-test-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "product"
        self.repo = self.root / "references/pi"
        self.repo.mkdir(parents=True)
        git(self.repo, "init", "--quiet", "--initial-branch=main")
        git(self.repo, "remote", "add", "origin", "https://example.invalid/pi.git")
        (self.repo / "README.md").write_text("reference source\n")
        (self.repo / "LICENSE").write_text("fixture license\n")
        (self.repo / "package-lock.json").write_text('{"lockfileVersion":3}\n')
        (self.repo / "link").symlink_to("README.md")
        git(self.repo, "add", ".")
        git(self.repo, "-c", "user.name=Pangu Test", "-c", "user.email=test@example.invalid",
            "-c", "commit.gpgsign=false", "commit", "--quiet", "-m", "fixture")
        self.commit = git(self.repo, "rev-parse", "HEAD").decode().strip()
        self.baseline = self.root / "baseline.json"
        self.policy = {"schema": "pangu.migration.reference-baseline.v1", "repositories": [{
            "id": "pi", "path": "references/pi", "commit": self.commit,
            "origin": "https://example.invalid/pi.git", "key_files": [
                {"path": "README.md", "sha256": digest(self.repo / "README.md")},
                {"path": "absent.json", "sha256": None},
            ],
        }]}
        self.baseline.write_text(json.dumps(self.policy))
        self.output = self.base / "frozen"

    def test_offline_restore_includes_links_licenses_and_locks(self):
        manifest = create(self.root, self.baseline, self.output)
        repo = manifest["repositories"][0]
        self.assertEqual(repo["file_count"], 4)
        self.assertIn("LICENSE", repo["license_files"])
        self.assertIn("package-lock.json", repo["lockfiles"])
        self.assertEqual(verify(self.output), manifest)
        self.assertEqual(git(self.repo, "status", "--porcelain"), b"")

    def test_dirty_tracked_file_is_rejected(self):
        (self.repo / "README.md").write_text("changed\n")
        with self.assertRaisesRegex(ValueError, "changes"):
            create(self.root, self.baseline, self.output)
        self.assertFalse(self.output.exists())

    def test_untracked_file_is_rejected(self):
        (self.repo / "hidden.ts").write_text("untracked")
        with self.assertRaisesRegex(ValueError, "changes"):
            snapshot(self.repo, self.commit)

    def test_ignored_file_is_rejected(self):
        (self.repo / ".git/info/exclude").write_text("ignored\n")
        (self.repo / "ignored").write_text("not reference source")
        with self.assertRaisesRegex(ValueError, "ignored files"):
            snapshot(self.repo, self.commit)

    def test_assume_unchanged_cannot_hide_modified_bytes(self):
        git(self.repo, "update-index", "--assume-unchanged", "README.md")
        (self.repo / "README.md").write_text("concealed change\n")
        with self.assertRaisesRegex(ValueError, "bytes differ"):
            snapshot(self.repo, self.commit)

    def test_changed_key_digest_is_rejected(self):
        self.policy["repositories"][0]["key_files"][0]["sha256"] = "0" * 64
        self.baseline.write_text(json.dumps(self.policy))
        with self.assertRaisesRegex(ValueError, "key-file mismatch"):
            create(self.root, self.baseline, self.output)

    def test_changed_head_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "HEAD changed"):
            snapshot(self.repo, "0" * 40)

    def test_declared_crlf_checkout_uses_canonical_git_bytes(self):
        (self.repo / ".gitattributes").write_text("*.bat text eol=crlf\n")
        (self.repo / "test.bat").write_bytes(b"echo hello\r\n")
        git(self.repo, "add", ".gitattributes", "test.bat")
        git(self.repo, "-c", "user.name=Pangu Test", "-c", "user.email=test@example.invalid",
            "-c", "commit.gpgsign=false", "commit", "--quiet", "-m", "CRLF fixture")
        commit = git(self.repo, "rev-parse", "HEAD").decode().strip()
        files = snapshot(self.repo, commit)
        batch = next(f for f in files if f["path"] == "test.bat")
        self.assertEqual(batch["size"], len(b"echo hello\n"))

    def test_unmaterialized_filter_is_preserved_as_git_blob(self):
        (self.repo / ".gitattributes").write_text("README.md filter=untrusted\n")
        git(self.repo, "add", ".gitattributes")
        git(self.repo, "-c", "user.name=Pangu Test", "-c", "user.email=test@example.invalid",
            "-c", "commit.gpgsign=false", "commit", "--quiet", "-m", "filter fixture")
        commit = git(self.repo, "rev-parse", "HEAD").decode().strip()
        files = snapshot(self.repo, commit)
        readme = next(f for f in files if f["path"] == "README.md")
        self.assertEqual(readme["checkout_filter"], "untrusted")
        self.assertEqual(readme["sha256"], digest(self.repo / "README.md"))

    def test_origin_mismatch_is_rejected(self):
        git(self.repo, "remote", "set-url", "origin", "https://example.invalid/other.git")
        with self.assertRaisesRegex(ValueError, "origin changed"):
            create(self.root, self.baseline, self.output)

    def test_bundle_corruption_is_rejected(self):
        create(self.root, self.baseline, self.output)
        with (self.output / "pi.bundle").open("ab") as target:
            target.write(b"corrupt")
        with self.assertRaisesRegex(ValueError, "hash/type mismatch"):
            verify(self.output)

    def test_inventory_corruption_is_rejected(self):
        create(self.root, self.baseline, self.output)
        (self.output / "pi.files.json").write_text("[]\n")
        with self.assertRaisesRegex(ValueError, "hash/type mismatch"):
            verify(self.output)

    def test_existing_bundle_is_never_overwritten(self):
        self.output.mkdir()
        (self.output / "keep").write_text("retained")
        with self.assertRaisesRegex(ValueError, "refusing to overwrite"):
            create(self.root, self.baseline, self.output)
        self.assertEqual((self.output / "keep").read_text(), "retained")

    def test_output_inside_product_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "outside the product"):
            create(self.root, self.baseline, self.root / "bundle")

    def test_manifest_path_escape_is_rejected(self):
        create(self.root, self.baseline, self.output)
        manifest_path = self.output / "manifest.json"
        manifest = json.loads(manifest_path.read_text())
        manifest["repositories"][0]["bundle"] = "../escape.bundle"
        manifest_path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "unsafe relative path"):
            verify(self.output)

    def test_missing_baseline_returns_failed_cli(self):
        result = subprocess.run([
            sys.executable, str(Path(__file__).with_name("reference_bundle.py")),
            "create", "--root", str(self.root), "--baseline", str(self.base / "missing.json"),
            "--output", str(self.output),
        ], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn("REFERENCE FAIL", result.stdout)


if __name__ == "__main__":
    unittest.main()
