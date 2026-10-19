"""Candidate G01 manifest preflight; never grants production admission."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath


class Invalid(ValueError):
    pass


def validate(value):
    if not isinstance(value, dict) or set(value) != {"schema", "modules"}:
        raise Invalid("unknown manifest fields")
    if value["schema"] != "pangu.modules.v1" or not isinstance(value["modules"], list):
        raise Invalid("invalid schema or modules")
    modules, owners = {}, {}
    for item in value["modules"]:
        if not isinstance(item, dict) or set(item) != {"id", "role", "scope", "files", "public_headers", "dependencies"}:
            raise Invalid("invalid module fields")
        name = item["id"]
        if not isinstance(name, str) or not name or name in modules:
            raise Invalid("invalid or duplicate module ID")
        if item["role"] not in {"host", "interface", "platform", "foundation", "plugin", "client"}:
            raise Invalid("unknown module role")
        if item["scope"] not in {"product", "test", "development"}:
            raise Invalid("unknown dependency scope")
        for field in ("files", "public_headers", "dependencies"):
            data = item[field]
            if not isinstance(data, list) or any(not isinstance(x, str) or not x for x in data) or len(data) != len(set(data)):
                raise Invalid("invalid or duplicate " + field)
        if not item["files"]:
            raise Invalid("empty module")
        for path in item["files"]:
            p = PurePosixPath(path)
            if p.is_absolute() or str(p) != path or ".." in p.parts or "\\" in path or p.parts[0] in {"references", ".git"}:
                raise Invalid("unsafe file path")
            if p.suffix not in {".c", ".h", ".S", ".asm", ".cpp", ".hpp"}:
                raise Invalid("unregistered language")
            if path in owners:
                raise Invalid("multiple file owners")
            owners[path] = name
        if not set(item["public_headers"]) <= set(item["files"]) or any(PurePosixPath(p).suffix not in {".h", ".hpp"} for p in item["public_headers"]):
            raise Invalid("invalid public headers")
        modules[name] = item
    for name, item in modules.items():
        for dependency in item["dependencies"]:
            if dependency not in modules:
                raise Invalid("unknown dependency")
            target = modules[dependency]
            if item["scope"] == "product" and target["scope"] != "product":
                raise Invalid("product depends on non-product module")
            if item["role"] == "interface" and target["role"] != "interface":
                raise Invalid("interface depends on implementation")
            if item["role"] == "host" and target["role"] not in {"interface", "foundation", "platform"}:
                raise Invalid("host depends on business or client")
    active, done = set(), set()

    def visit(name):
        if name in active:
            raise Invalid("dependency cycle")
        if name in done:
            return
        active.add(name)
        for dependency in modules[name]["dependencies"]:
            visit(dependency)
        active.remove(name)
        done.add(name)

    for name in modules:
        visit(name)
    return modules, owners


def check_include(manifest, source, header):
    """Check an actual dependency pair; collection from compiler remains required."""
    modules, owners = validate(manifest)
    if source not in owners or header not in owners:
        raise Invalid("unknown compiler input")
    consumer, provider = owners[source], owners[header]
    if consumer == provider:
        return
    if provider not in modules[consumer]["dependencies"]:
        raise Invalid("undeclared include dependency")
    if header not in modules[provider]["public_headers"]:
        raise Invalid("private header dependency")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()
    raw = args.manifest.read_bytes()
    try:
        modules, _ = validate(json.loads(raw))
    except (Invalid, ValueError, TypeError, KeyError) as error:
        print("G01 manifest FAIL:", error)
        return 1
    print(json.dumps({"status": "BLOCKED", "manifest_preflight": "PASS", "modules": len(modules),
                      "manifest_sha256": hashlib.sha256(raw).hexdigest(),
                      "missing": ["compiler dependency collection", "link and service graph evidence",
                                  "G02 fixed build toolchain", "independent protected policy approval"]}))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
