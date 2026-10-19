"""Reproduce the fixed, test-only Assembly/C toolchain experiment outside the repo."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from compile_inputs import digest
from native_build import build, verify_build
from receipt import canonical


ASSEMBLY = '''.intel_syntax noprefix
.text
.global probe_start
.type probe_start, @function
probe_start:
    .cfi_startproc
    .cfi_undefined rip
    call answer
    mov edi, eax
    mov eax, 60
    syscall
    .cfi_endproc
.size probe_start, .-probe_start
.section .note.GNU-stack,"",@progbits
'''
C_SOURCE = "int answer(void) { return 7; }\n"


def experiment(profile, profile_pin, directory):
    directory = Path(directory).absolute()
    repository = Path(__file__).resolve().parents[2]
    if directory.resolve().is_relative_to(repository):
        raise ValueError("experiment must be outside repository")
    directory.mkdir()
    root = directory / "source"
    root.mkdir()
    (root / "entry.S").write_text(ASSEMBLY)
    (root / "answer.c").write_text(C_SOURCE)
    manifest = {"schema": "pangu.modules.v1", "modules": [
        {"id": "entry", "role": "platform", "scope": "test", "files": ["entry.S"], "public_headers": [], "dependencies": ["answer"]},
        {"id": "answer", "role": "foundation", "scope": "test", "files": ["answer.c"], "public_headers": [], "dependencies": []}]}
    exports = {"entry": ["probe_start"], "answer": ["answer"]}
    (root / "modules.json").write_bytes(canonical(manifest) + b"\n")
    (root / "exports.json").write_bytes(canonical(exports) + b"\n")
    (directory / "toolchain.json").write_bytes(canonical(profile) + b"\n")
    runs = {}
    for mode in ("debug", "release"):
        for repetition in (1, 2):
            label = mode + str(repetition)
            output = directory / label
            report = build(root, manifest, exports, profile, profile_pin, mode, "probe_start", output)
            receipt_pin = digest(output / "build.json")
            binding = verify_build(root, manifest, exports, profile, profile_pin, mode, "probe_start", output, receipt_pin)
            executable = output / "probe.elf"
            executable.chmod(0o700)
            result = subprocess.run([str(executable)], capture_output=True, timeout=5, env={"LC_ALL": "C"})
            if result.returncode != 7 or result.stdout or result.stderr:
                raise ValueError("fixed native test fixture returned unexpected result")
            runs[label] = {"receipt_sha256": receipt_pin, "elf_sha256": report["artifacts"]["probe.elf"],
                           "exit_code": result.returncode, "artifact_verification": binding}
        if runs[mode + "1"]["elf_sha256"] != runs[mode + "2"]["elf_sha256"]:
            raise ValueError("same-mode native ELF is not repeatable")
    debug = directory / "debug1"
    command = [profile["tools"]["debugger"]["path"], "-nx", "-nh", "-batch", "-iex", "set auto-load off",
               "-ex", "set debuginfod enabled off", "-ex", "set disable-randomization off", "-ex", "set startup-with-shell off",
               "-ex", "info line answer", "-ex", "info line probe_start", "-ex", "break answer", "-ex", "run", "-ex", "bt",
               str(debug / "probe.elf")]
    cache = directory / "debug-cache"
    cache.mkdir()
    gdb = subprocess.run(command, cwd=root, env={"PATH": "/usr/bin:/bin", "LC_ALL": "C", "XDG_CACHE_HOME": str(cache)},
                         capture_output=True, text=True, timeout=15)
    if (gdb.returncode or not re.search(r'Line 1 of "(?:\./)?answer\.c"', gdb.stdout)
            or not re.search(r'Line \d+ of "(?:\./)*entry\.S"', gdb.stdout)
            or not re.search(r'#0\s+.*answer', gdb.stdout) or not re.search(r'#1\s+.*probe_start', gdb.stdout)):
        raise ValueError("debug line probe failed: " + gdb.stdout + gdb.stderr)
    (directory / "debugger.log").write_text(gdb.stdout + gdb.stderr)
    # Exercise PIC objects in a DSO link too; the probe is not a plugin loader test.
    report = json.loads((debug / "build.json").read_text())
    objects = [str(debug / n) for n in report["artifacts"] if n.endswith(".o")]
    command_shared = [profile["tools"]["linker"]["path"], "--fatal-warnings", "--no-undefined", "-shared", "-z", "noexecstack", "-o", str(directory / "probe.so"), *objects]
    shared = subprocess.run(command_shared, capture_output=True, text=True, timeout=15, env={"PATH": "/usr/bin:/bin", "LC_ALL": "C"})
    if shared.returncode or shared.stderr:
        raise ValueError("PIC shared link failed: " + shared.stderr)
    summary = {"schema": "pangu.toolchain-experiment.v1", "status": "BLOCKED", "fixture_scope": "test-only",
               "native_runs": runs, "same_mode_elf_repeatability": "PASS", "debug_lines": "PASS",
               "assembly_debug_lines": "PASS", "assembly_c_backtrace": "PASS",
               "debugger_command": command, "debugger_log_sha256": digest(directory / "debugger.log"),
               "pic_shared_link": "PASS", "shared_link_command": command_shared, "shared_sha256": digest(directory / "probe.so"),
               "toolchain_profile_sha256": profile_pin, "release": "BLOCKED",
               "missing": ["independently approved production toolchain", "hermetic image", "cross-host reproduction", "plugin DSO lifecycle and ABI"]}
    (directory / "experiment.json").write_bytes(canonical(summary) + b"\n")
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--toolchain-sha256", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        profile = json.loads(args.toolchain.read_text())
        if hashlib.sha256(canonical(profile)).hexdigest() != args.toolchain_sha256:
            raise ValueError("experiment toolchain pin mismatch")
        experiment(profile, args.toolchain_sha256, args.output)
    except (ValueError, OSError, TypeError, KeyError, subprocess.SubprocessError) as error:
        print("toolchain experiment FAIL:", error)
        return 1
    print(json.dumps({"status": "BLOCKED", "experiment": str(args.output / "experiment.json"),
                      "experiment_sha256": digest(args.output / "experiment.json")}))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
