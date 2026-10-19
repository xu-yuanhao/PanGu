"""Candidate native input audit. No link, ABI or release admission implied."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from module_graph import Invalid, check_include, validate

C_FLAGS = ["-x", "c", "-std=c17", "-nostdinc", "-Wall", "-Wextra", "-Werror", "-pedantic-errors", "-I", "."]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(root, owners):
    found = {}
    for directory, dirs, files in os.walk(root, followlinks=False):
        base = Path(directory)
        if base == root:
            dirs[:] = [d for d in dirs if d not in {".git", "references"}]
        for name in dirs + files:
            path = base / name
            if path.is_symlink():
                raise Invalid("symlink in candidate tree")
        for name in files:
            path = base / name
            relative = path.relative_to(root).as_posix()
            if path.suffix in {".c", ".h", ".S", ".asm", ".cpp", ".hpp"}:
                if relative not in owners:
                    raise Invalid("unknown native input: " + relative)
                if any(c.isspace() or c in "#:$\\" for c in relative):
                    raise Invalid("unsupported dependency path")
                found[relative] = digest(path)
    if set(found) != set(owners):
        raise Invalid("registered native file missing")
    return found


def audit(root, manifest, compiler, compiler_sha256, link_probe=None, *, language_flags=None, inspect_objects=None, assembler_probe=None):
    root = Path(root).resolve(strict=True)
    compiler = Path(compiler).resolve(strict=True)
    if digest(compiler) != compiler_sha256:
        raise Invalid("compiler digest mismatch")
    _, owners = validate(manifest)
    before = inventory(root, owners)
    sources = sorted(p for p in owners if Path(p).suffix not in {".h", ".hpp"})
    if not sources:
        raise Invalid("BLOCKED: no native translation units")
    # Only this C17 probe is implemented. Other dialects remain blocked until T06.
    flags = language_flags if language_flags is not None else {".c": C_FLAGS}
    if any(Path(p).suffix not in flags for p in sources):
        raise Invalid("BLOCKED: unsupported translation unit language")
    environment = {"PATH": "/usr/bin:/bin", "LC_ALL": "C"}
    commands, dependencies, objects = [], {}, {}
    link_evidence = {}
    assembly_records = {}
    assembler = None
    if assembler_probe is not None:
        if set(assembler_probe) != {"path", "sha256", "flags"}:
            raise Invalid("invalid assembler policy")
        assembler = Path(assembler_probe["path"]).resolve(strict=True)
        if digest(assembler) != assembler_probe["sha256"]:
            raise Invalid("assembler digest mismatch")
    with tempfile.TemporaryDirectory(prefix="pangu-input-audit-") as temporary:
        object_paths = {}
        for index, source in enumerate(sources):
            obj = Path(temporary) / f"{index}.o"
            dep = Path(temporary) / f"{index}.d"
            staged = Path(temporary) / f"{index}.s"
            language = Path(source).suffix
            action = "-c" if assembler is None else ("-E" if language == ".S" else "-S")
            command = [str(compiler)] + flags[Path(source).suffix] + ["-MD", "-MF", str(dep), "-MT", "pangu",
                       action, "./" + source, "-o", str(obj if assembler is None else staged)]
            result = subprocess.run(command, cwd=root, env=environment, capture_output=True, text=True, timeout=30)
            if result.returncode:
                raise Invalid("compile failed: " + source + "\n" + result.stderr)
            if result.stderr.strip():
                raise Invalid("unexpected compiler diagnostics: " + result.stderr)
            commands.append(command)
            data = dep.read_text().replace("\\\n", " ")
            if not data.startswith("pangu:"):
                raise Invalid("invalid compiler dependency output")
            tokens = data[len("pangu:"):].split()
            if assembler is not None:
                assembly_dep = Path(temporary) / f"{index}.asm.d"
                normalization = None
                if language == ".c":
                    # Clang emits an unnumbered STT_FILE basename in addition to
                    # fully qualified DWARF .file rows. GAS --MD lists that row
                    # as a dependency, even though it never opens the basename.
                    # Normalize this one producer-owned row to the actual input;
                    # never drop/guess dependencies emitted for real file reads.
                    assembly_text = staged.read_text()
                    metadata = re.search(r'(?m)^\s*\.file\s+("(?:[^"\\]|\\.)*")\s*$', assembly_text)
                    if metadata is None or json.loads(metadata[1]) != Path(source).name:
                        raise Invalid("unexpected compiler file metadata")
                    normalization = {"kind": "compiler-primary-file-path", "before_sha256": digest(staged),
                                     "from": Path(source).name, "to": source}
                    staged.write_text(assembly_text[:metadata.start()] + "\t.file\t" + json.dumps(source, ensure_ascii=False)
                                      + assembly_text[metadata.end():])
                intermediate_sha = digest(staged)
                assemble = [str(assembler)] + assembler_probe["flags"][language] + [
                    "--debug-prefix-map=" + str(root) + "=.", "--debug-prefix-map=" + temporary + "=/pangu-build",
                    "--MD", str(assembly_dep), str(staged), "-o", str(obj)]
                assembled = subprocess.run(assemble, cwd=root, env=environment, capture_output=True, text=True, timeout=30)
                if assembled.returncode or assembled.stderr.strip():
                    raise Invalid("assembly failed: " + source + "\n" + assembled.stderr)
                if digest(staged) != intermediate_sha:
                    raise Invalid("generated assembly changed during assembly")
                as_data = assembly_dep.read_text().replace("\\\n", " ")
                if ":" not in as_data:
                    raise Invalid("invalid assembler dependency output")
                tokens += as_data.split(":", 1)[1].split()
                assembly_records[source] = {"intermediate_sha256": intermediate_sha, "command": assemble,
                                            "compiler_dependencies_raw": data, "assembler_dependencies_raw": as_data,
                                            "metadata_normalization": normalization,
                                            "stdout": assembled.stdout, "stderr": assembled.stderr}
                commands.append(assemble)
            inputs = []
            for token in tokens:
                path = (root / token).resolve(strict=True)
                if assembler is not None and path == staged:
                    continue
                if not path.is_relative_to(root):
                    raise Invalid("compiler input outside candidate")
                relative = path.relative_to(root).as_posix()
                check_include(manifest, source, relative)
                inputs.append(relative)
            if source not in inputs:
                raise Invalid("compiler omitted translation unit")
            dependencies[source] = sorted(set(inputs))
            objects[source] = digest(obj)
            object_paths[source] = obj
        if link_probe is not None:
            from link_graph import audit_objects
            if set(link_probe) != {"nm", "nm_sha256", "linker", "linker_sha256", "exports"}:
                raise Invalid("invalid link probe policy")
            tools = {}
            for name in ("nm", "linker"):
                tool = Path(link_probe[name]).resolve(strict=True)
                if digest(tool) != link_probe[name + "_sha256"]:
                    raise Invalid(name + " digest mismatch")
                tools[name] = tool
            linked = Path(temporary) / "combined.o"
            link_evidence = audit_objects(manifest, link_probe["exports"], object_paths,
                                          tools["nm"], tools["linker"], linked)
            link_evidence["linked_object_sha256"] = digest(linked)
            link_evidence["tool_sha256"] = {name: digest(tool) for name, tool in tools.items()}
            for name, tool in tools.items():
                if digest(tool) != link_probe[name + "_sha256"]:
                    raise Invalid("link tool changed during audit")
        if inspect_objects is not None:
            link_evidence["final_build"] = inspect_objects(object_paths, Path(temporary))
    if inventory(root, owners) != before or digest(compiler) != compiler_sha256:
        raise Invalid("input changed during audit")
    if assembler is not None and digest(assembler) != assembler_probe["sha256"]:
        raise Invalid("assembler changed during audit")
    return {"status": "BLOCKED", "compile_input_audit": "PASS", "source_sha256": before,
            "manifest_sha256": hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest(),
            "compiler_sha256": compiler_sha256, "commands": commands, "dependencies": dependencies,
            "object_sha256": objects, "link_evidence": link_evidence, "assembly_records": assembly_records,
            "missing": ["final link and runtime service graph", "approved T06 toolchain",
                                                     "protected executor and independent policy approval"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--compiler-sha256", required=True)
    parser.add_argument("--link-probe", type=Path, help="candidate tool pins and per-module public symbol policy")
    args = parser.parse_args()
    try:
        probe = json.loads(args.link_probe.read_text()) if args.link_probe else None
        report = audit(args.root, json.loads(args.manifest.read_text()), args.compiler, args.compiler_sha256, probe)
    except (Invalid, OSError, ValueError, subprocess.SubprocessError) as error:
        print("native input audit FAIL:", error)
        return 1
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
