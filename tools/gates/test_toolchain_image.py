from pathlib import Path
import tempfile
import unittest

from receipt import canonical, sha
from toolchain_image import dependencies, verify


class ToolImageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.image = self.base / "image"
        path = self.image / "rootfs/usr/bin/tool"
        path.parent.mkdir(parents=True)
        path.write_bytes(b"tool bytes")
        path.chmod(0o700)
        launcher = self.base / "launcher"
        launcher.write_bytes(b"launcher bytes")
        self.report = {"schema": "pangu.local-tool-image.v1", "status": "CANDIDATE_NOT_APPROVED",
                       "files": {"/usr/bin/tool": {"sha256": sha(path.read_bytes()), "mode": 0o700}}, "file_count": 1,
                       "bwrap": {"path": str(launcher), "sha256": sha(launcher.read_bytes())}}

    def save(self):
        data = canonical(self.report)
        (self.image / "image.json").write_bytes(data)
        return sha(data)

    def test_loader_dependency_parser(self):
        text = "linux-vdso.so.1 (0x1234)\nlibc.so.6 => /usr/lib/libc.so.6 (0x4567)\n/usr/lib/ld-linux-x86-64.so.2 (0x5678)\n"
        self.assertEqual(dependencies(text), {Path("/usr/lib/libc.so.6"), Path("/usr/lib/ld-linux-x86-64.so.2")})
        self.assertEqual(dependencies("statically linked\n"), set())

    def test_unresolved_library_rejected(self):
        with self.assertRaisesRegex(ValueError, "unresolved"):
            dependencies("libc.so.6 => not found\n")

    def test_image_file_binding(self):
        self.assertEqual(verify(self.image, self.save())["file_count"], 1)

    def test_changed_image_file(self):
        pin = self.save()
        (self.image / "rootfs/usr/bin/tool").write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "file changed"):
            verify(self.image, pin)

    def test_file_permission_change(self):
        pin = self.save()
        (self.image / "rootfs/usr/bin/tool").chmod(0o600)
        with self.assertRaisesRegex(ValueError, "file changed"):
            verify(self.image, pin)

    def test_missing_image_file(self):
        pin = self.save()
        (self.image / "rootfs/usr/bin/tool").unlink()
        with self.assertRaisesRegex(ValueError, "file missing"):
            verify(self.image, pin)

    def test_extra_image_file(self):
        pin = self.save()
        (self.image / "rootfs/usr/bin/hidden").write_bytes(b"extra")
        with self.assertRaisesRegex(ValueError, "unknown tool image"):
            verify(self.image, pin)

    def test_changed_launcher(self):
        pin = self.save()
        (self.base / "launcher").write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "launcher changed"):
            verify(self.image, pin)

    def test_empty_image_is_not_success(self):
        self.report["files"] = {}
        self.report["file_count"] = 0
        with self.assertRaisesRegex(ValueError, "empty tool image"):
            verify(self.image, self.save())

    def test_symlink_image_file(self):
        pin = self.save()
        path = self.image / "rootfs/usr/bin/tool"
        path.unlink()
        path.symlink_to(self.base / "launcher")
        with self.assertRaisesRegex(ValueError, "symlink"):
            verify(self.image, pin)


if __name__ == "__main__":
    unittest.main()
