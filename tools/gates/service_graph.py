"""Static root-scope service composition preflight; no plugin code is executed."""
import argparse
import hashlib
import json
from pathlib import Path

from module_graph import Invalid, validate


def fields(value, expected):
    if not isinstance(value, dict) or set(value) != set(expected):
        raise Invalid("unknown or missing service fields")


def identifier(value):
    if not isinstance(value, str) or not value or value.strip() != value:
        raise Invalid("invalid service identifier")
    return value


def version(value):
    if type(value) is not int or value < 1:
        raise Invalid("service ABI version must be a positive integer")


def resolve(manifest, composition):
    modules, _ = validate(manifest)
    fields(composition, {"schema", "services", "plugins", "profile"})
    if composition["schema"] != "pangu.service-composition.v1":
        raise Invalid("unknown service schema")
    if not isinstance(composition["services"], list) or not isinstance(composition["plugins"], list):
        raise Invalid("invalid service or plugin list")
    services, plugins = {}, {}
    for service in composition["services"]:
        fields(service, {"id", "version", "definition_module"})
        key = identifier(service["id"])
        version(service["version"])
        if key in services:
            raise Invalid("duplicate service ID")
        owner = service["definition_module"]
        if not isinstance(owner, str) or owner not in modules or modules[owner]["role"] != "interface":
            raise Invalid("service definition must belong to interface module")
        services[key] = service
    for plugin in composition["plugins"]:
        fields(plugin, {"module", "provides", "requires"})
        module = identifier(plugin["module"])
        if module not in modules or module in plugins or modules[module]["role"] == "interface":
            raise Invalid("unknown, duplicate or interface plugin module")
        if not isinstance(plugin["provides"], list) or not isinstance(plugin["requires"], list):
            raise Invalid("invalid provides or requires")
        for relation in ("provides", "requires"):
            seen = set()
            for entry in plugin[relation]:
                expected = {"service", "version"} if relation == "provides" else {"service", "version", "optional", "on_missing"}
                fields(entry, expected)
                key = identifier(entry["service"])
                version(entry["version"])
                if key not in services or key in seen:
                    raise Invalid("unknown or duplicate service relation")
                seen.add(key)
                if entry["version"] != services[key]["version"]:
                    raise Invalid("service ABI version conflict")
                if services[key]["definition_module"] not in modules[module]["dependencies"]:
                    raise Invalid("service interface dependency undeclared")
                if relation == "requires":
                    if type(entry["optional"]) is not bool:
                        raise Invalid("optional must be boolean")
                    expected_missing = "disable-feature" if entry["optional"] else "pending"
                    if entry["on_missing"] != expected_missing:
                        raise Invalid("missing dependency behavior not explicit or unsupported")
        plugins[module] = plugin
    profile = composition["profile"]
    fields(profile, {"id", "scope", "modules", "bindings"})
    identifier(profile["id"])
    if profile["scope"] not in {"product", "test", "development"}:
        raise Invalid("unknown profile scope")
    selected = profile["modules"]
    if not isinstance(selected, list) or any(not isinstance(x, str) for x in selected) or len(selected) != len(set(selected)):
        raise Invalid("invalid selected modules")
    if not selected:
        raise Invalid("BLOCKED: empty service composition")
    for module in selected:
        if module not in plugins:
            raise Invalid("selected module has no plugin declaration")
        if profile["scope"] == "product" and modules[module]["scope"] != "product":
            raise Invalid("product profile selects non-product plugin")
    bindings = profile["bindings"]
    if not isinstance(bindings, dict):
        raise Invalid("invalid bindings")
    available = {}
    for module in selected:
        for entry in plugins[module]["provides"]:
            available.setdefault(entry["service"], []).append(module)
    # All selected offers must have one explicit binding; no first-loaded winner.
    if set(bindings) != set(available):
        raise Invalid("bindings must cover exactly selected service offers")
    for service, provider in bindings.items():
        if not isinstance(provider, str) or provider not in available[service]:
            raise Invalid("binding provider does not offer selected service")
        if len(available[service]) != 1:
            raise Invalid("multiple providers in root scope; deselect alternatives")
    graph = {module: set() for module in selected}
    edges, missing_optional, pending = [], [], []
    for module in selected:
        for entry in plugins[module]["requires"]:
            service = entry["service"]
            provider = bindings.get(service)
            if provider is None:
                record = {"module": module, "service": service}
                (missing_optional if entry["optional"] else pending).append(record)
                continue
            graph[module].add(provider)
            edges.append({"consumer": module, "service": service, "provider": provider})
    active, done, order = set(), set(), []

    def visit(module):
        if module in active:
            raise Invalid("service activation cycle")
        if module in done:
            return
        active.add(module)
        for dependency in sorted(graph[module]):
            visit(dependency)
        active.remove(module)
        done.add(module)
        order.append(module)

    for module in sorted(graph):
        visit(module)
    return {"status": "BLOCKED", "service_manifest_preflight": "PENDING" if pending else "PASS",
            "required_missing": pending, "optional_disabled": missing_optional, "service_edges": edges,
            "planned_activation_order": [] if pending else order,
            "planned_disposal_order": [] if pending else list(reversed(order)),
            "composition_sha256": hashlib.sha256(json.dumps(composition, sort_keys=True).encode()).hexdigest(),
            "modules_sha256": hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest(),
            "missing": ["native registration and binding trace", "activation, failure and disposal runtime tests",
                        "protected policy approval"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--composition", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = resolve(json.loads(args.manifest.read_text()), json.loads(args.composition.read_text()))
    except (Invalid, ValueError, TypeError, KeyError, OSError) as error:
        print("service graph FAIL:", error)
        return 1
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
