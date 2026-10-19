import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from module_graph import Invalid
from native_build import build, inspect_elf, validate_native_profile, verify_build
from compile_inputs import digest
from receipt import canonical
from toolchain_probe import ASSEMBLY as START


class NativeBuildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "source"
        self.root.mkdir()
        (self.root / "entry.S").write_text(START)
        (self.root / "answer.c").write_text("int answer(void) { return 7; }\n")
        self.manifest = {"schema": "pangu.modules.v1", "modules": [
            {"id": "entry", "role": "platform", "scope": "test", "files": ["entry.S"], "public_headers": [], "dependencies": ["answer"]},
            {"id": "answer", "role": "foundation", "scope": "test", "files": ["answer.c"], "public_headers": [], "dependencies": []}]}
        self.exports = {"entry": ["probe_start"], "answer": ["answer"]}
        self.profile = json.loads((Path(__file__).resolve().parents[2] / "governance/native-probe-toolchain.json").read_text())

    def run_build(self, mode="debug", directory="build"):
        pin = hashlib.sha256(canonical(self.profile)).hexdigest()
        return build(self.root, self.manifest, self.exports, self.profile, pin, mode, "probe_start", self.base / directory)

    def test_debug_and_release_assembly_c_execute(self):
        for mode in ("debug", "release"):
            with self.subTest(mode=mode):
                report = self.run_build(mode, mode)
                executable = self.base / mode / "probe.elf"
                executable.chmod(0o700)
                result = subprocess.run([str(executable)], capture_output=True, timeout=5)
                self.assertEqual(result.returncode, 7)
                self.assertEqual(result.stdout, b"")
                self.assertEqual(report["native_static_build"], "PASS")
                self.assertEqual(report["status"], "BLOCKED")
                self.assertIn("probe.map", report["artifacts"])

    def test_repeated_build_identical_outputs(self):
        for mode in ("debug", "release"):
            first = self.run_build(mode, mode + "1")
            second = self.run_build(mode, mode + "2")
            self.assertEqual(first["artifacts"]["probe.elf"], second["artifacts"]["probe.elf"])

    def test_pic_objects_link_shared(self):
        report = self.run_build()
        objects = [str(self.base / "build" / name) for name in report["artifacts"] if name.endswith(".o")]
        result = subprocess.run([self.profile["tools"]["linker"]["path"], "--fatal-warnings", "--no-undefined",
                                 "-shared", "-z", "noexecstack", "-o", str(self.base / "probe.so"), *objects],
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertTrue((self.base / "probe.so").is_file())

    def test_nested_c_file_metadata_keeps_real_source_dependency(self):
        folder = self.root / "src/answer"
        folder.mkdir(parents=True)
        (self.root / "answer.c").rename(folder / "answer.c")
        self.manifest["modules"][1]["files"] = ["src/answer/answer.c"]
        report = self.run_build()
        self.assertIn("src/answer/answer.c", report["dependencies"]["src/answer/answer.c"])
        record = report["assembly_records"]["src/answer/answer.c"]
        self.assertEqual(record["metadata_normalization"]["to"], "src/answer/answer.c")
        self.assertIn("src/answer/answer.c", record["assembler_dependencies_raw"])

    def test_syscall_in_plugin_rejected(self):
        self.manifest["modules"][0]["role"] = "plugin"
        with self.assertRaisesRegex(Invalid, "platform instruction"):
            self.run_build()
        self.assertFalse((self.base / "build/build.json").exists())

    def test_unknown_assembly_include_rejected(self):
        (self.root / "extra.h").write_text("#define EXIT 60\n")
        (self.root / "entry.S").write_text('#include "extra.h"\n' + START)
        with self.assertRaisesRegex(Invalid, "unknown native input"):
            self.run_build()

    def test_assembler_include_not_in_cpp_dependencies_rejected(self):
        (self.root / "unknown.inc").write_text("nop\n")
        (self.root / "entry.S").write_text('.include "unknown.inc"\n' + START)
        with self.assertRaisesRegex(Invalid, "unknown compiler input"):
            self.run_build()

    def test_assembler_binary_input_rejected(self):
        (self.root / "payload.bin").write_bytes(b"\x90")
        (self.root / "entry.S").write_text('.incbin "payload.bin"\n' + START)
        with self.assertRaisesRegex(Invalid, "unknown compiler input"):
            self.run_build()

    def test_inline_asm_cannot_hide_binary_input(self):
        (self.root / "payload.bin").write_bytes(b"\x90")
        (self.root / "answer.c").write_text(r'''__asm__(".pushsection .rodata\n.incbin \"payload.bin\"\n.popsection\n");
int answer(void) { return 7; }
''')
        with self.assertRaisesRegex(Invalid, "unknown compiler input"):
            self.run_build()

    def test_declared_assembler_input_recorded(self):
        (self.root / "part.h").write_text(".equ sample_constant, 7\n")
        self.manifest["modules"][0]["files"].append("part.h")
        (self.root / "entry.S").write_text('.include "part.h"\n' + START)
        report = self.run_build()
        self.assertIn("part.h", report["dependencies"]["entry.S"])
        self.assertIn("part.h", report["assembly_records"]["entry.S"]["assembler_dependencies_raw"])

    def test_unknown_dialect_rejected(self):
        path = self.root / "entry.S"
        path.rename(self.root / "entry.asm")
        self.manifest["modules"][0]["files"] = ["entry.asm"]
        with self.assertRaisesRegex(Invalid, "unsupported translation unit"):
            self.run_build()

    def test_missing_entry_rejected(self):
        (self.root / "entry.S").write_text(START.replace("probe_start", "different_entry"))
        self.exports["entry"] = ["different_entry"]
        with self.assertRaisesRegex(Invalid, "tool failed"):
            self.run_build()

    def test_no_output_inside_sources(self):
        with self.assertRaisesRegex(Invalid, "outside candidate"):
            self.run_build(directory="source/output")

    def test_no_overwrite(self):
        self.run_build()
        with self.assertRaisesRegex(Invalid, "already exists"):
            self.run_build()

    def test_bad_profile_flags(self):
        self.profile["modes"]["release"][".c"].append("-w")
        with self.assertRaisesRegex(Invalid, "unsupported native"):
            validate_native_profile(self.profile)

    def test_changed_disassembler(self):
        self.profile["tools"]["disassembler"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(Invalid, "disassembler identity"):
            self.run_build()

    def check_build(self, receipt_pin):
        return verify_build(self.root, self.manifest, self.exports, self.profile,
                            hashlib.sha256(canonical(self.profile)).hexdigest(), "debug", "probe_start",
                            self.base / "build", receipt_pin)

    def test_independent_retained_artifact_recheck(self):
        self.run_build()
        report = self.check_build(digest(self.base / "build/build.json"))
        self.assertEqual(report["native_artifact_binding"], "PASS")
        self.assertEqual(report["retained_symbol_recheck"], "PASS")
        self.assertEqual(report["status"], "BLOCKED")

    def test_changed_source_invalidates_native_receipt(self):
        self.run_build()
        pin = digest(self.base / "build/build.json")
        (self.root / "answer.c").write_text("int answer(void) { return 8; }\n")
        with self.assertRaisesRegex(Invalid, "source or policy changed"):
            self.check_build(pin)

    def test_artifact_tampering_rejected(self):
        report = self.run_build()
        pin = digest(self.base / "build/build.json")
        for name in ("probe.elf", "probe.map", "disassembly.json", next(n for n in report["artifacts"] if n.endswith(".o"))):
            path = self.base / "build" / name
            original = path.read_bytes()
            path.write_bytes(original + b"tamper")
            with self.subTest(name=name), self.assertRaisesRegex(Invalid, "artifact changed"):
                self.check_build(pin)
            path.write_bytes(original)

    def test_receipt_tampering_rejected(self):
        self.run_build()
        path = self.base / "build/build.json"
        pin = digest(path)
        path.write_text('{}')
        with self.assertRaisesRegex(Invalid, "receipt digest"):
            self.check_build(pin)

    def test_missing_or_extra_artifact_rejected(self):
        self.run_build()
        pin = digest(self.base / "build/build.json")
        (self.base / "build/hidden.so").write_bytes(b"extra")
        with self.assertRaisesRegex(Invalid, "inventory mismatch"):
            self.check_build(pin)
        (self.base / "build/hidden.so").unlink()
        (self.base / "build/probe.map").unlink()
        with self.assertRaisesRegex(Invalid, "inventory mismatch"):
            self.check_build(pin)

    def test_malformed_elf(self):
        path = self.base / "bad.elf"
        path.write_bytes(b"not an ELF")
        with self.assertRaisesRegex(Invalid, "ELF64"):
            inspect_elf(path)

    def test_elf_security_rejections(self):
        self.run_build()
        original = (self.base / "build/probe.elf").read_bytes()
        offset = struct.unpack_from("<Q", original, 32)[0]
        count = struct.unpack_from("<H", original, 56)[0]
        for attack in ("dynamic", "rwx", "stack", "entry", "truncated"):
            data = bytearray(original)
            if attack == "entry":
                struct.pack_into("<Q", data, 24, 0)
            elif attack == "truncated":
                data = data[:64]
            else:
                for index in range(count):
                    where = offset + 56 * index
                    kind = struct.unpack_from("<I", data, where)[0]
                    if attack == "dynamic" and kind == 1:
                        struct.pack_into("<I", data, where, 3)
                        break
                    if attack == "rwx" and kind == 1:
                        struct.pack_into("<I", data, where + 4, 7)
                        break
                    if attack == "stack" and kind == 0x6474e551:
                        struct.pack_into("<I", data, where + 4, 7)
                        break
            path = self.base / (attack + ".elf")
            path.write_bytes(data)
            with self.subTest(attack=attack), self.assertRaises(Invalid):
                inspect_elf(path)


if __name__ == "__main__":
    unittest.main()
