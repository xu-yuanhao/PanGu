"""Actual trap execution and failure-closed sanitizer policy/source binding tests."""
import copy
import hashlib
from pathlib import Path
import signal
import subprocess
import unittest

from module_graph import Invalid
from native_build import (build, verify_build, validate_native_profile, UBSAN_MODES, CPP_ASSEMBLER_MODES)
from receipt import canonical
import test_native_build as native_fixture


class NativeUbsanTests(unittest.TestCase):
    def setUp(self):
        native_fixture.NativeBuildTests.setUp(self)
        self.profile.update(schema="pangu.native-ubsan-trap-toolchain.v3", modes=copy.deepcopy(UBSAN_MODES),
                            assembler_modes=copy.deepcopy(CPP_ASSEMBLER_MODES))

    def configure(self, body, cpp=False):
        for path in self.root.glob("answer.*"):
            path.unlink()
        name = "answer.cpp" if cpp else "answer.c"
        (self.root / name).write_text(body)
        self.manifest["modules"][1]["files"] = [name]

    def run_build(self, mode="debug", directory="build"):
        return native_fixture.NativeBuildTests.run_build(self, mode, directory)

    def execute(self, directory):
        source = self.base / directory / "probe.elf"
        exe = self.base / (directory + "-exec")
        exe.write_bytes(source.read_bytes())
        exe.chmod(0o700)
        # Prevent intentional negative probes from leaving cores behind.
        import resource
        return subprocess.run([str(exe)], capture_output=True, timeout=5,
                              preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))

    def test_healthy_c_cpp_modes_and_retained_ast_binding(self):
        for cpp in (False, True):
            self.configure(('extern "C" ' if cpp else '') + 'int answer(void) {return 7;}\n', cpp)
            for mode in ("debug", "release"):
                directory = ("cpp-" if cpp else "c-") + mode
                with self.subTest(cpp=cpp, mode=mode):
                    report = self.run_build(mode, directory)
                    self.assertEqual(self.execute(directory).returncode, 7)
                    audited = report["link_evidence"]["final_build"]["ubsan_sources"]
                    self.assertEqual(set(audited), {"answer.cpp" if cpp else "answer.c"})
                    receipt = self.base / directory / "build.json"
                    checked = verify_build(self.root, self.manifest, self.exports, self.profile,
                                           hashlib.sha256(canonical(self.profile)).hexdigest(), mode,
                                           "probe_start", receipt.parent, hashlib.sha256(receipt.read_bytes()).hexdigest())
                    self.assertEqual(checked["native_artifact_binding"], "PASS")

    def test_c_cpp_overflow_traps_both_modes(self):
        for cpp in (False, True):
            self.configure(('extern "C" ' if cpp else '') + 'int answer(void) {volatile int x=2147483647;return x+1;}\n', cpp)
            for mode in ("debug", "release"):
                directory = ("cpp-" if cpp else "c-") + mode
                self.run_build(mode, directory)
                with self.subTest(cpp=cpp, mode=mode):
                    result = self.execute(directory)
                    self.assertEqual(result.returncode, -signal.SIGILL)
                    self.assertEqual(result.stdout, b"")
                    self.assertEqual(result.stderr, b"")

    def test_shift_bounds_and_misalignment_trap(self):
        probes = [
            'int answer(void) {volatile unsigned int n=32;return (int)(1U<<n);}\n',
            'int answer(void) {int a[2]={1,2};volatile int i=2;return a[i];}\n',
            '_Alignas(8) static unsigned char bytes[16];int answer(void) {volatile unsigned int offset=1;int *p=(int *)(bytes+offset);return *p;}\n',
        ]
        for i, source in enumerate(probes):
            self.configure(source)
            self.run_build(directory="fault" + str(i))
            self.assertEqual(self.execute("fault" + str(i)).returncode, -signal.SIGILL)

    def test_wrong_indirect_function_type_traps(self):
        self.configure('extern "C" __attribute__((noinline)) long long other(void) {return 7;}\n'
                       'extern "C" int answer(void) {long long (*source)(void)=other;int (*volatile fn)(void)=nullptr;auto *out=reinterpret_cast<volatile unsigned char *>(&fn);auto *in=reinterpret_cast<const unsigned char *>(&source);for(unsigned long i=0;i<sizeof(fn);i++)out[i]=in[i];return fn();}\n', True)
        self.run_build()
        self.assertEqual(self.execute("build").returncode, -signal.SIGILL)

    def test_attribute_names_in_literals_are_not_attributes(self):
        self.configure('int answer(void) {const char *text="NoSanitizeAttr DisableSanitizerInstrumentationAttr";return text[0]==78 ? 7 : 0;}\n')
        self.run_build()
        self.assertEqual(self.execute("build").returncode, 7)

    def test_changed_header_invalidates_audited_unit(self):
        self.configure('#include "extra.h"\nDECORATE int answer(void) {return 7;}\n')
        self.manifest["modules"][1]["files"].append("extra.h")
        (self.root / "extra.h").write_text('#define DECORATE\n')
        self.run_build(directory="before")
        (self.root / "extra.h").write_text('#define DECORATE __attribute__((no_sanitize("undefined")))\n')
        with self.assertRaisesRegex(Invalid, "suppression attribute forbidden"):
            self.run_build(directory="after")

    def test_has_include_change_invalidates_audited_unit(self):
        self.configure('#if __has_include("toggle.h")\n#define DECORATE __attribute__((no_sanitize("undefined")))\n#else\n#define DECORATE\n#endif\nDECORATE int answer(void) {return 7;}\n')
        self.run_build(directory="before")
        (self.root / "toggle.h").write_text('/* Existence changes preprocessing without an actual include. */\n')
        self.manifest["modules"][1]["files"].append("toggle.h")
        with self.assertRaisesRegex(Invalid, "suppression attribute forbidden"):
            self.run_build(directory="after")

    def test_source_suppression_attribute_rejected(self):
        self.configure('__attribute__((no_sanitize("undefined"))) int answer(void) {volatile int x=2147483647;return x+1;}\n')
        with self.assertRaisesRegex(Invalid, "suppression attribute forbidden"):
            self.run_build()

    def test_macro_expanded_suppression_rejected(self):
        self.configure('#define SKIP no_sanitize\n#define QUIET __attribute__((SKIP("undefined")))\nQUIET int answer(void) {return 7;}\n')
        with self.assertRaisesRegex(Invalid, "suppression attribute forbidden"):
            self.run_build()

    def test_disable_instrumentation_attribute_rejected(self):
        self.configure('__attribute__((disable_sanitizer_instrumentation)) int answer(void) {return 7;}\n')
        with self.assertRaisesRegex(Invalid, "suppression attribute forbidden"):
            self.run_build()

    def test_sanitizer_flags_cannot_be_removed_or_recovered(self):
        for flag in ("-fsanitize=undefined", "-fsanitize-trap=all", "-fno-sanitize-recover=all"):
            value = copy.deepcopy(self.profile)
            value["modes"]["debug"][".c"].remove(flag)
            with self.subTest(flag=flag), self.assertRaisesRegex(Invalid, "unsupported native language"):
                validate_native_profile(value)
        value = copy.deepcopy(self.profile)
        value["modes"]["debug"][".cpp"].append("-fno-sanitize=function")
        with self.assertRaisesRegex(Invalid, "unsupported native language"):
            validate_native_profile(value)

    def test_forged_or_missing_retained_sanitizer_evidence_rejected(self):
        import json
        self.run_build()
        path = self.base / "build/build.json"
        record = json.loads(path.read_text())
        record["link_evidence"]["final_build"]["ubsan_sources"] = {}
        path.write_bytes(canonical(record))
        with self.assertRaisesRegex(Invalid, "sanitizer source evidence mismatch"):
            verify_build(self.root, self.manifest, self.exports, self.profile,
                         hashlib.sha256(canonical(self.profile)).hexdigest(), "debug", "probe_start",
                         path.parent, hashlib.sha256(path.read_bytes()).hexdigest())


if __name__ == "__main__":
    unittest.main()
