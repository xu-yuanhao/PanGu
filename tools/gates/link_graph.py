"""Closed-world static object-link probe, without system libraries or DSOs."""
from pathlib import Path
import subprocess

from module_graph import Invalid, validate


def run(command):
    result = subprocess.run(command, env={"PATH": "/usr/bin:/bin", "LC_ALL": "C"},
                            capture_output=True, text=True, timeout=30)
    if result.returncode or result.stderr.strip():
        raise Invalid("symbol/link tool failed: " + result.stderr)
    return result.stdout


def symbols(nm, path):
    defined, undefined = set(), set()
    for line in run([str(nm), "--format=posix", "--extern-only", str(path)]).splitlines():
        fields = line.split()
        if len(fields) < 2:
            raise Invalid("unrecognized symbol output")
        name, kind = fields[:2]
        if kind == "U":
            undefined.add(name)
        elif kind in {"T", "D", "B", "R", "S", "G"}:
            defined.add(name)
        else:
            # Weak, common, IFUNC and unique symbols need explicit future semantics.
            raise Invalid("unsupported symbol class: " + kind)
    return defined, undefined


def audit_objects(manifest, exports, objects, nm, linker, output):
    modules, owners = validate(manifest)
    if not isinstance(exports, dict) or set(exports) != set(modules):
        raise Invalid("export policy must cover exactly all modules")
    for names in exports.values():
        if not isinstance(names, list) or any(not isinstance(n, str) or not n for n in names) or len(names) != len(set(names)):
            raise Invalid("invalid export policy")
    expected = {p for p in owners if Path(p).suffix not in {".h", ".hpp"}}
    if not objects or set(objects) != expected:
        raise Invalid("object coverage mismatch")
    definitions, references, module_definitions = {}, {}, {m: set() for m in modules}
    for source, obj in objects.items():
        defined, undefined = symbols(nm, obj)
        references[source] = undefined
        for symbol in defined:
            if symbol in definitions:
                raise Invalid("duplicate global definition: " + symbol)
            definitions[symbol] = source
            module_definitions[owners[source]].add(symbol)
    for module, names in exports.items():
        if not set(names) <= module_definitions[module]:
            raise Invalid("declared export missing: " + module)
    edges = []
    for source, names in references.items():
        consumer = owners[source]
        for symbol in sorted(names):
            if symbol not in definitions:
                raise Invalid("unresolved external symbol: " + symbol)
            provider_source = definitions[symbol]
            provider = owners[provider_source]
            if consumer != provider:
                if provider not in modules[consumer]["dependencies"]:
                    raise Invalid("undeclared symbol dependency: " + symbol)
                if symbol not in exports[provider]:
                    raise Invalid("private symbol dependency: " + symbol)
            edges.append({"source": source, "symbol": symbol, "provider_source": provider_source})
    # A relocatable link exercises the real linker without running candidate code.
    command = [str(linker), "--fatal-warnings", "-r", "-o", str(output)] + [str(objects[s]) for s in sorted(objects)]
    run(command)
    _, unresolved = symbols(nm, output)
    if unresolved:
        raise Invalid("linked object retains unresolved symbols")
    return {"symbol_link_audit": "PASS", "symbol_edges": edges, "link_command": command,
            "exports": exports, "limitations": ["closed-world relocatable link only",
                                                "no final ELF, DSO, runtime service or ABI validation"]}
