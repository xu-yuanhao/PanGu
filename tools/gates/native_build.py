"""Assembly/C static ELF build probe, for external test trees only until admission."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

from compile_inputs import C_FLAGS, audit, digest, inventory
from link_graph import audit_objects, run, symbols
from module_graph import Invalid, validate
from receipt import canonical
from toolchain import output, validate_profile


C_NATIVE = ["-x", "c", "-std=c17", "-nostdinc", "-Wall", "-Wextra", "-Werror", "-pedantic-errors",
            "-I", ".", "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-fvisibility=hidden", "-fPIC",
            "-fno-integrated-as", "-fno-addrsig"]
ASM_NATIVE = ["-x", "assembler-with-cpp", "-nostdinc", "-I", ".", "-Werror"]
MODES = {"debug": {".c": C_NATIVE + ["-O0", "-g"], ".S": ASM_NATIVE},
         "release": {".c": C_NATIVE + ["-Oz"], ".S": ASM_NATIVE}}
ASSEMBLER_MODES = {"debug": {".c": ["--64", "--fatal-warnings"], ".S": ["--64", "--fatal-warnings", "--gdwarf-5"]},
                   "release": {".c": ["--64", "--fatal-warnings"], ".S": ["--64", "--fatal-warnings"]}}
FINAL_FLAGS = ["--fatal-warnings", "--no-undefined", "--build-id=none", "-static", "-z", "noexecstack", "-z", "relro", "-z", "now"]


# Version 2 adds only a restricted test-consumer dialect; version 1 stays byte-for-byte policy compatible.
CPP_NATIVE = ["-x", "c++", "-std=c++20", "-nostdinc", "-nostdinc++", "-Wall", "-Wextra", "-Werror", "-pedantic-errors",
              "-I", ".", "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-fvisibility=hidden", "-fPIC",
              "-fno-integrated-as", "-fno-addrsig", "-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics",
              "-fno-unwind-tables", "-fno-asynchronous-unwind-tables"]
CPP_MODES = {mode: dict(languages, **{".cpp": CPP_NATIVE + (["-O0", "-g"] if mode == "debug" else ["-Oz"])})
             for mode, languages in MODES.items()}
CPP_ASSEMBLER_MODES = {mode: dict(languages, **{".cpp": languages[".c"][:]})
                       for mode, languages in ASSEMBLER_MODES.items()}


UBSAN_FLAGS = ["-fsanitize=undefined", "-fsanitize-trap=all", "-fno-sanitize-recover=all"]
UBSAN_MODES = {mode: {language: flags + (UBSAN_FLAGS if language in {".c", ".cpp"} else [])
                      for language, flags in languages.items()} for mode, languages in CPP_MODES.items()}


def profile_modes(profile):
    if profile["schema"] == "pangu.native-probe-toolchain.v1":
        return MODES, ASSEMBLER_MODES
    if profile["schema"] == "pangu.native-test-cpp-toolchain.v2":
        return CPP_MODES, CPP_ASSEMBLER_MODES
    if profile["schema"] == "pangu.native-ubsan-trap-toolchain.v3":
        return UBSAN_MODES, CPP_ASSEMBLER_MODES
    raise Invalid("unsupported native toolchain schema")


def inspect_cpp_object(path, nm, disassembler):
    text = run([str(disassembler), "--section-headers", str(path)])
    sections = {m[1] for line in text.splitlines() if (m := re.match(r"\s*\d+\s+(\S+)\s+[0-9a-fA-F]+\s", line))}
    if not sections or any(name.startswith((".init", ".fini", ".preinit_array", ".ctors", ".dtors", ".eh_frame",
                                           ".gcc_except_table", ".tdata", ".tbss")) for name in sections):
        raise Invalid("C++ test object has forbidden initialization, unwind or TLS section")
    defined, undefined = symbols(nm, path)
    if any(name.startswith("_Z") for name in defined | undefined):
        raise Invalid("C++ test boundary requires C-linkage global symbols")
    return {"cpp_test_object": "PASS", "sections": sorted(sections), "defined": sorted(defined), "undefined": sorted(undefined)}


def validate_native_profile(profile):
    if not isinstance(profile, dict) or set(profile) != {"schema", "status", "host", "compiler_target", "modes", "assembler_modes", "final_flags", "tools"}:
        raise Invalid("unknown native toolchain fields")
    modes, assembler_modes = profile_modes(profile)
    if (profile["modes"] != modes or profile["assembler_modes"] != assembler_modes or profile["final_flags"] != FINAL_FLAGS):
        raise Invalid("unsupported native language, mode or flags")
    if not isinstance(profile["tools"], dict) or set(profile["tools"]) != {"compiler", "assembler", "nm", "linker", "disassembler", "debugger"}:
        raise Invalid("incomplete native tools")
    base = {"schema": "pangu.c17-probe-toolchain.v1", "status": profile["status"], "host": profile["host"],
            "compiler_target": profile["compiler_target"], "compile_flags": C_FLAGS,
            "link_flags": ["--fatal-warnings", "-r"],
            "tools": {n: profile["tools"][n] for n in ("compiler", "nm", "linker")}}
    validate_profile(base)
    for name in ("assembler", "disassembler", "debugger"):
        spec = profile["tools"][name]
        if not isinstance(spec, dict) or set(spec) != {"path", "sha256", "version"}:
            raise Invalid("invalid " + name + " specification")
        path = Path(spec["path"])
        if not path.is_absolute() or path.resolve(strict=True) != path or digest(path) != spec["sha256"]:
            raise Invalid(name + " identity mismatch")
        if output([str(path), "--version"]) != spec["version"]:
            raise Invalid(name + " version mismatch")


def inspect_elf(path):
    data = path.read_bytes()
    if len(data) < 64 or data[:7] != b"\x7fELF\x02\x01\x01":
        raise Invalid("expected little-endian ELF64")
    header = struct.unpack_from("<HHIQQQIHHHHHH", data, 16)
    kind, machine, version, entry, phoff = header[:5]
    phsize, phnum = header[8:10]
    if kind != 2 or machine != 62 or version != 1 or phsize != 56 or not 0 < phnum < 128:
        raise Invalid("unsupported final ELF target or headers")
    if phoff < 64 or phoff + phsize * phnum > len(data):
        raise Invalid("truncated ELF program headers")
    stack, executable_entry, loads = False, False, 0
    for index in range(phnum):
        ptype, flags, offset, address, _, size, memory, _ = struct.unpack_from("<IIQQQQQQ", data, phoff + index * phsize)
        if ptype in {2, 3}:
            raise Invalid("dynamic loader or dynamic segment forbidden")
        if ptype == 0x6474e551:
            if flags & 1:
                raise Invalid("executable stack forbidden")
            stack = True
        if ptype == 1:
            loads += 1
            if flags & 3 == 3:
                raise Invalid("writable executable load segment forbidden")
            if size > memory or offset + size > len(data):
                raise Invalid("invalid ELF load segment")
            if flags & 1 and address <= entry < address + size:
                executable_entry = True
    if not stack or not executable_entry or loads == 0:
        raise Invalid("missing non-executable stack or valid entry")
    return {"class": "ELF64", "machine": "x86_64", "type": "EXEC", "entry": entry,
            "load_segments": loads, "interpreter": None, "writable_executable_segments": False, "executable_stack": False}


def build(root, manifest, exports, profile, profile_pin, mode, entry, destination):
    root, destination = Path(root).resolve(strict=True), Path(destination).absolute()
    if destination.resolve().is_relative_to(root):
        raise Invalid("build output must be outside candidate tree")
    if destination.exists() or destination.is_symlink():
        raise Invalid("build output already exists")
    if hashlib.sha256(canonical(profile)).hexdigest() != profile_pin:
        raise Invalid("native toolchain pin mismatch")
    validate_native_profile(profile)
    modes, assembler_modes = profile_modes(profile)
    if mode not in modes or not isinstance(entry, str) or not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", entry):
        raise Invalid("invalid build mode or entry symbol")
    modules, owners = validate(manifest)
    for source, owner in owners.items():
        if Path(source).suffix == ".cpp" and modules[owner]["scope"] != "test":
            raise Invalid("C++ native probe is restricted to test modules")
    tools = profile["tools"]
    link = {"exports": exports}
    for name in ("nm", "linker"):
        link[name], link[name + "_sha256"] = tools[name]["path"], tools[name]["sha256"]
    flags = {language: values + ["-ffile-prefix-map=" + str(root) + "=.", "-fdebug-compilation-dir=."]
             for language, values in modes[mode].items()}
    destination.mkdir()
    artifacts = {}

    def final(objects, temporary):
        instructions, retained, cpp, sanitizer = {}, {}, {}, {}
        for source, obj in objects.items():
            if profile["schema"] == "pangu.native-ubsan-trap-toolchain.v3" and Path(source).suffix in {".c", ".cpp"}:
                from sanitizer_audit import audit_source
                sanitizer[source] = audit_source(root, source, tools["compiler"]["path"], flags[Path(source).suffix], tools["compiler"]["sha256"])
            if Path(source).suffix == ".cpp":
                cpp[source] = inspect_cpp_object(obj, tools["nm"]["path"], tools["disassembler"]["path"])
            text = run([tools["disassembler"]["path"], "-d", "--no-show-raw-insn", "--x86-asm-syntax=intel", str(obj)])
            for line in text.splitlines():
                match = re.match(r"\s*[0-9a-f]+:\s+(syscall|sysenter|int)\b", line)
                if match and modules[owners[source]]["role"] not in {"host", "platform"}:
                    raise Invalid("platform instruction outside platform/bootstrap: " + source)
            instructions[source] = text
            target = destination / (str(len(artifacts)) + ".o")
            shutil.copyfile(obj, target)
            artifacts[target.name] = digest(target)
            retained[source] = target.name
        executable, linkmap = temporary / "probe.elf", temporary / "probe.map"
        command = [tools["linker"]["path"]] + FINAL_FLAGS + ["-e", entry, "-Map", str(linkmap), "-o", str(executable)] + [str(objects[s]) for s in sorted(objects)]
        run(command)
        elf = inspect_elf(executable)
        for source in (executable, linkmap):
            target = destination / source.name
            shutil.copyfile(source, target)
            artifacts[target.name] = digest(target)
        (destination / "disassembly.json").write_bytes(canonical(instructions) + b"\n")
        artifacts["disassembly.json"] = digest(destination / "disassembly.json")
        return {"elf": elf, "command": command, "instruction_audit": "PASS", "retained_objects": retained, "cpp_test_objects": cpp, "ubsan_sources": sanitizer}

    report = audit(root, manifest, tools["compiler"]["path"], tools["compiler"]["sha256"], link,
                   language_flags=flags, inspect_objects=final,
                   assembler_probe={"path": tools["assembler"]["path"], "sha256": tools["assembler"]["sha256"],
                                    "flags": assembler_modes[mode]})
    validate_native_profile(profile)
    report.update({"schema": "pangu.native-build-probe.v1", "mode": mode, "entry_symbol": entry, "toolchain_profile_sha256": profile_pin,
                   "toolchain_profile": profile, "artifacts": artifacts, "native_static_build": "PASS",
                   "runtime_execution": "NOT_RUN", "release": "BLOCKED"})
    (destination / "build.json").write_bytes(canonical(report) + b"\n")
    return report


def verify_build(root, manifest, exports, profile, profile_pin, mode, entry, destination, receipt_pin):
    root, destination = Path(root).resolve(strict=True), Path(destination).resolve(strict=True)
    path = destination / "build.json"
    if path.is_symlink() or digest(path) != receipt_pin:
        raise Invalid("native receipt digest mismatch")
    report = json.loads(path.read_text())
    validate_native_profile(profile)
    modules, owners = validate(manifest)
    if (report["schema"] != "pangu.native-build-probe.v1" or report["status"] != "BLOCKED"
            or report["release"] != "BLOCKED" or report["native_static_build"] != "PASS"
            or report["mode"] != mode or report["entry_symbol"] != entry
            or report["toolchain_profile"] != profile or hashlib.sha256(canonical(profile)).hexdigest() != profile_pin
            or report["toolchain_profile_sha256"] != profile_pin):
        raise Invalid("native build receipt context mismatch")
    if (report["source_sha256"] != inventory(root, owners)
            or report["manifest_sha256"] != hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()
            or report["link_evidence"]["exports"] != exports):
        raise Invalid("native source or policy changed")
    artifacts = report["artifacts"]
    if not isinstance(artifacts, dict) or not {"probe.elf", "probe.map", "disassembly.json"} <= set(artifacts):
        raise Invalid("missing required native artifact")
    expected_files = set(artifacts) | {"build.json"}
    if {p.name for p in destination.iterdir()} != expected_files:
        raise Invalid("native artifact inventory mismatch")
    for name, expected in artifacts.items():
        if Path(name).name != name or name in {".", ".."}:
            raise Invalid("unsafe native artifact path")
        artifact = destination / name
        if artifact.is_symlink() or not artifact.is_file() or digest(artifact) != expected:
            raise Invalid("native artifact changed: " + name)
    final = report["link_evidence"]["final_build"]
    if inspect_elf(destination / "probe.elf") != final["elf"]:
        raise Invalid("final ELF evidence mismatch")
    if any(Path(name).name != name or name not in artifacts for name in final["retained_objects"].values()):
        raise Invalid("unsafe retained object path")
    objects = {source: destination / name for source, name in final["retained_objects"].items()}
    for source, obj in objects.items():
        if obj.name not in artifacts or digest(obj) != report["object_sha256"][source]:
            raise Invalid("retained object does not match compilation")
    checked_sanitizer = {}
    if profile["schema"] == "pangu.native-ubsan-trap-toolchain.v3":
        from sanitizer_audit import audit_source
        mode_flags, _ = profile_modes(profile)
        for source in objects:
            language = Path(source).suffix
            if language in {".c", ".cpp"}:
                values = mode_flags[mode][language] + ["-ffile-prefix-map=" + str(root) + "=.", "-fdebug-compilation-dir=."]
                checked_sanitizer[source] = audit_source(root, source, profile["tools"]["compiler"]["path"], values, profile["tools"]["compiler"]["sha256"])
    if checked_sanitizer != final.get("ubsan_sources", {}):
        raise Invalid("retained sanitizer source evidence mismatch")
    checked_cpp = {}
    for source, obj in objects.items():
        if Path(source).suffix == ".cpp":
            if modules[owners[source]]["scope"] != "test":
                raise Invalid("C++ native probe is restricted to test modules")
            checked_cpp[source] = inspect_cpp_object(obj, profile["tools"]["nm"]["path"], profile["tools"]["disassembler"]["path"])
    if checked_cpp != final.get("cpp_test_objects", {}):
        raise Invalid("retained C++ test object evidence mismatch")
    with tempfile.TemporaryDirectory(prefix="pangu-link-recheck-") as temporary:
        checked = audit_objects(manifest, exports, objects, profile["tools"]["nm"]["path"],
                                profile["tools"]["linker"]["path"], Path(temporary) / "combined.o")
    if checked["symbol_edges"] != report["link_evidence"]["symbol_edges"]:
        raise Invalid("retained symbol evidence mismatch")
    return {"native_artifact_binding": "PASS", "retained_symbol_recheck": "PASS", "release": "BLOCKED", "status": "BLOCKED"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "manifest", "exports", "toolchain", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--toolchain-sha256", required=True)
    parser.add_argument("--mode", choices=sorted(MODES), required=True)
    parser.add_argument("--entry", required=True)
    parser.add_argument("--verify-receipt-sha256", help="recheck existing output instead of building")
    args = parser.parse_args()
    try:
        arguments = (args.root, json.loads(args.manifest.read_text()), json.loads(args.exports.read_text()),
                     json.loads(args.toolchain.read_text()), args.toolchain_sha256, args.mode, args.entry, args.output)
        if args.verify_receipt_sha256:
            print(json.dumps(verify_build(*arguments, args.verify_receipt_sha256)))
            return 2
        report = build(*arguments)
    except (Invalid, ValueError, TypeError, KeyError, OSError, subprocess.SubprocessError) as error:
        print("native build FAIL:", error)
        return 1
    print(json.dumps({"status": "BLOCKED", "native_static_build": report["native_static_build"],
                      "receipt": str(args.output / "build.json"), "receipt_sha256": digest(args.output / "build.json")}))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
