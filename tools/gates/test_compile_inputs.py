import json
from pathlib import Path
import shutil
import tempfile
import unittest

from compile_inputs import audit, digest
from module_graph import Invalid


class CompilerInputTests(unittest.TestCase):
    def setUp(self):
        compiler = shutil.which("clang")
        if compiler is None:
            self.fail("clang required; missing tool must not silently skip native gate tests")
        self.compiler = Path(compiler).resolve()
        self.pin = digest(self.compiler)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "api").mkdir()
        (self.root / "loop").mkdir()
        (self.root / "api/public.h").write_text("int api_value(void);\n")
        (self.root / "api/private.h").write_text("int internal_value(void);\n")
        (self.root / "loop/main.c").write_text('#include "api/public.h"\nint run(void) { return api_value(); }\n')
        self.manifest = {"schema": "pangu.modules.v1", "modules": [
            {"id": "api", "role": "interface", "scope": "product", "files": ["api/public.h", "api/private.h"],
             "public_headers": ["api/public.h"], "dependencies": []},
            {"id": "loop", "role": "plugin", "scope": "product", "files": ["loop/main.c"],
             "public_headers": [], "dependencies": ["api"]}]}

    def run_audit(self):
        return audit(self.root, self.manifest, self.compiler, self.pin)

    def test_compiler_generated_evidence(self):
        report = self.run_audit()
        self.assertEqual(report["compile_input_audit"], "PASS")
        self.assertEqual(report["status"], "BLOCKED")
        self.assertEqual(report["dependencies"]["loop/main.c"], ["api/public.h", "loop/main.c"])
        self.assertEqual(len(report["object_sha256"]["loop/main.c"]), 64)
        json.dumps(report)

    def test_source_starting_with_dash_is_not_compiler_option(self):
        (self.root / "loop/main.c").rename(self.root / "--version.c")
        self.manifest["modules"][1]["files"] = ["--version.c"]
        report = self.run_audit()
        self.assertEqual(report["compile_input_audit"], "PASS")
        self.assertIn("./--version.c", report["commands"][0])

    def test_private_include(self):
        (self.root / "loop/main.c").write_text('#include "api/private.h"\nint run(void) { return internal_value(); }\n')
        with self.assertRaisesRegex(Invalid, "private header"):
            self.run_audit()

    def test_transitive_private_include(self):
        (self.root / "api/public.h").write_text('#include "api/private.h"\nint api_value(void);\n')
        with self.assertRaisesRegex(Invalid, "private header"):
            self.run_audit()

    def test_undeclared_dependency(self):
        self.manifest["modules"][1]["dependencies"] = []
        with self.assertRaisesRegex(Invalid, "undeclared"):
            self.run_audit()

    def test_unknown_source(self):
        (self.root / "hidden.c").write_text("int hidden;\n")
        with self.assertRaisesRegex(Invalid, "unknown native input"):
            self.run_audit()

    def test_unregistered_header(self):
        (self.root / "hidden.h").write_text("int hidden;\n")
        with self.assertRaisesRegex(Invalid, "unknown native input"):
            self.run_audit()

    def test_missing_file(self):
        (self.root / "api/private.h").unlink()
        with self.assertRaisesRegex(Invalid, "missing"):
            self.run_audit()

    def test_symlink(self):
        (self.root / "alias").symlink_to(self.root / "api", target_is_directory=True)
        with self.assertRaisesRegex(Invalid, "symlink"):
            self.run_audit()

    def test_compiler_pin(self):
        with self.assertRaisesRegex(Invalid, "compiler digest"):
            audit(self.root, self.manifest, self.compiler, "0" * 64)

    def test_diagnostics_fail(self):
        (self.root / "loop/main.c").write_text("int run(void) { int unused; return 1; }\n")
        with self.assertRaisesRegex(Invalid, "compile failed"):
            self.run_audit()

    def test_empty_not_pass(self):
        with tempfile.TemporaryDirectory() as empty:
            with self.assertRaisesRegex(Invalid, "no native translation"):
                audit(empty, {"schema": "pangu.modules.v1", "modules": []}, self.compiler, self.pin)

    def test_unregistered_external_include(self):
        with tempfile.TemporaryDirectory() as external:
            header = Path(external) / "outside.h"
            header.write_text("int api_value(void);\n")
            (self.root / "loop/main.c").write_text(f'#include "{header}"\nint run(void) {{ return api_value(); }}\n')
            with self.assertRaisesRegex(Invalid, "outside candidate"):
                self.run_audit()


if __name__ == "__main__":
    unittest.main()
