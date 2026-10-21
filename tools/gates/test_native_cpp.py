"""Restricted test-only C++20 dialect: real compile, link, rejection and receipt checks."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import unittest

from module_graph import Invalid
from native_build import (CPP_MODES, CPP_ASSEMBLER_MODES, build, verify_build,
                          validate_native_profile)
from receipt import canonical
import test_native_build as native_fixture


class NativeCppTests(unittest.TestCase):
    def setUp(self):
        native_fixture.NativeBuildTests.setUp(self)
        (self.root / "answer.c").unlink()
        (self.root / "answer.cpp").write_text('extern "C" int answer() { return 7; }\n')
        self.manifest["modules"][1]["files"] = ["answer.cpp"]
        self.v1 = copy.deepcopy(self.profile)
        self.profile["schema"] = "pangu.native-test-cpp-toolchain.v2"
        self.profile["modes"] = copy.deepcopy(CPP_MODES)
        self.profile["assembler_modes"] = copy.deepcopy(CPP_ASSEMBLER_MODES)

    def run_build(self, mode="debug", directory="build"):
        return native_fixture.NativeBuildTests.run_build(self, mode, directory)

    def test_real_cpp_debug_release_execute_and_verify(self):
        for mode in ("debug", "release"):
            report = self.run_build(mode, mode)
            directory = self.base / mode
            checked = report["link_evidence"]["final_build"]["cpp_test_objects"]
            self.assertEqual(set(checked), {"answer.cpp"})
            self.assertEqual(checked["answer.cpp"]["defined"], ["answer"])
            self.assertEqual(checked["answer.cpp"]["undefined"], [])
            result = verify_build(self.root, self.manifest, self.exports, self.profile,
                                  hashlib.sha256(canonical(self.profile)).hexdigest(), mode,
                                  "probe_start", directory, hashlib.sha256((directory / "build.json").read_bytes()).hexdigest())
            self.assertEqual(result["native_artifact_binding"], "PASS")
            exe = self.base / (mode + "-exec")
            exe.write_bytes((directory / "probe.elf").read_bytes())
            exe.chmod(0o700)
            self.assertEqual(subprocess.run([str(exe)], timeout=5).returncode, 7)
            self.assertEqual(report["release"], "BLOCKED")

    def test_old_v1_still_rejects_cpp(self):
        self.profile = self.v1
        with self.assertRaisesRegex(Invalid, "unsupported translation unit language"):
            self.run_build()

    def test_cpp_product_module_rejected(self):
        self.manifest["modules"][1]["scope"] = "product"
        with self.assertRaisesRegex(Invalid, "restricted to test"):
            self.run_build()

    def test_cpp_development_module_rejected(self):
        self.manifest["modules"][1]["scope"] = "development"
        with self.assertRaisesRegex(Invalid, "restricted to test"):
            self.run_build()

    def test_removing_cpp_safety_flag_rejected(self):
        for flag in ("-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics", "-fno-unwind-tables", "-nostdinc++"):
            profile = copy.deepcopy(self.profile)
            profile["modes"]["debug"][".cpp"].remove(flag)
            with self.subTest(flag=flag), self.assertRaisesRegex(Invalid, "unsupported native language"):
                validate_native_profile(profile)

    def test_cpp_profile_c_flags_cannot_drift(self):
        self.profile["modes"]["release"][".c"].remove("-Werror")
        with self.assertRaisesRegex(Invalid, "unsupported native language"):
            validate_native_profile(self.profile)

    def test_implicit_global_constructor_rejected(self):
        (self.root / "answer.cpp").write_text('static int state;\n__attribute__((constructor)) static void boot() {state=7;}\nextern "C" int answer() {return state;}\n')
        with self.assertRaisesRegex(Invalid, "forbidden initialization"):
            self.run_build()

    def test_preinitialization_section_rejected(self):
        (self.root / "answer.cpp").write_text('asm(".section .preinit_array,\\\"aw\\\",@preinit_array\\n.quad 0\\n.text");\nextern "C" int answer() {return 7;}\n')
        with self.assertRaisesRegex(Invalid, "forbidden initialization"):
            self.run_build()

    def test_tls_rejected(self):
        (self.root / "answer.cpp").write_text('static thread_local int value;\nextern "C" int answer() {return ++value;}\n')
        with self.assertRaises(Invalid):
            self.run_build()

    def test_mangled_global_boundary_rejected(self):
        (self.root / "answer.cpp").write_text('int helper() {return 7;}\nextern "C" int answer() {return helper();}\n')
        with self.assertRaisesRegex(Invalid, "C-linkage global symbols"):
            self.run_build()

    def test_exception_use_rejected(self):
        (self.root / "answer.cpp").write_text('extern "C" int answer() {throw 7;}\n')
        with self.assertRaisesRegex(Invalid, "compile failed"):
            self.run_build()

    def test_standard_library_include_rejected(self):
        (self.root / "answer.cpp").write_text('#include <vector>\nextern "C" int answer() {return 7;}\n')
        with self.assertRaisesRegex(Invalid, "compile failed"):
            self.run_build()

    def test_missing_cpp_receipt_evidence_rejected(self):
        self.run_build()
        path = self.base / "build/build.json"
        report = json.loads(path.read_text())
        report["link_evidence"]["final_build"]["cpp_test_objects"] = {}
        path.write_bytes(canonical(report))
        with self.assertRaisesRegex(Invalid, "C\\+\\+ test object evidence mismatch"):
            verify_build(self.root, self.manifest, self.exports, self.profile,
                         hashlib.sha256(canonical(self.profile)).hexdigest(), "debug", "probe_start",
                         path.parent, hashlib.sha256(path.read_bytes()).hexdigest())


if __name__ == "__main__":
    unittest.main()
