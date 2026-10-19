from pathlib import Path
import shutil
import unittest

from compile_inputs import audit, digest
from module_graph import Invalid
import test_compile_inputs


class LinkGraphTests(unittest.TestCase):
    setUp = test_compile_inputs.CompilerInputTests.setUp

    def prepare(self):
        (self.root / "provider").mkdir()
        (self.root / "provider/value.c").write_text("int api_value(void) { return 7; }\nint private_value(void) { return 9; }\n")
        self.manifest["modules"].append({"id": "provider", "role": "foundation", "scope": "product",
                                         "files": ["provider/value.c"], "public_headers": [], "dependencies": []})
        self.manifest["modules"][1]["dependencies"].append("provider")
        self.probe = {"exports": {"api": [], "loop": ["run"], "provider": ["api_value"]}}
        for name, executable in (("nm", "nm"), ("linker", "ld")):
            tool = shutil.which(executable)
            self.assertIsNotNone(tool, executable + " required")
            path = Path(tool).resolve()
            self.probe[name] = str(path)
            self.probe[name + "_sha256"] = digest(path)

    def run_link(self):
        return audit(self.root, self.manifest, self.compiler, self.pin, self.probe)

    def test_actual_relocatable_link(self):
        self.prepare()
        report = self.run_link()
        evidence = report["link_evidence"]
        self.assertEqual(report["status"], "BLOCKED")
        self.assertEqual(evidence["symbol_link_audit"], "PASS")
        self.assertEqual(evidence["symbol_edges"], [{"source": "loop/main.c", "symbol": "api_value",
                                                   "provider_source": "provider/value.c"}])
        self.assertEqual(len(evidence["linked_object_sha256"]), 64)

    def test_direct_extern_cannot_bypass_private_boundary(self):
        self.prepare()
        (self.root / "loop/main.c").write_text("extern int private_value(void);\nint run(void) { return private_value(); }\n")
        with self.assertRaisesRegex(Invalid, "private symbol"):
            self.run_link()

    def test_undeclared_symbol_provider(self):
        self.prepare()
        self.manifest["modules"][1]["dependencies"].remove("provider")
        with self.assertRaisesRegex(Invalid, "undeclared symbol"):
            self.run_link()

    def test_missing_export(self):
        self.prepare()
        self.probe["exports"]["provider"].append("nonexistent")
        with self.assertRaisesRegex(Invalid, "declared export missing"):
            self.run_link()

    def test_duplicate_definition(self):
        self.prepare()
        (self.root / "loop/main.c").write_text("int api_value(void) { return 1; }\n")
        with self.assertRaisesRegex(Invalid, "duplicate global"):
            self.run_link()

    def test_unresolved_os_call(self):
        self.prepare()
        (self.root / "loop/main.c").write_text("extern int system(const char *);\nint run(void) { return system(0); }\n")
        with self.assertRaisesRegex(Invalid, "unresolved external.*system"):
            self.run_link()

    def test_weak_symbols_rejected(self):
        self.prepare()
        (self.root / "provider/value.c").write_text("__attribute__((weak)) int api_value(void) { return 7; }\n")
        with self.assertRaisesRegex(Invalid, "unsupported symbol class"):
            self.run_link()

    def test_tool_pin(self):
        self.prepare()
        self.probe["nm_sha256"] = "0" * 64
        with self.assertRaisesRegex(Invalid, "nm digest mismatch"):
            self.run_link()

    def test_export_coverage(self):
        self.prepare()
        del self.probe["exports"]["api"]
        with self.assertRaisesRegex(Invalid, "cover exactly"):
            self.run_link()


if __name__ == "__main__":
    unittest.main()
