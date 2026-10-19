"""Preview native source admission under a separately pinned controller.

The project's design G00 remains unchanged. This entry builds candidate matrices,
never executes candidate programs and never grants production or release approval.
"""
import argparse
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import tempfile

from module_graph import Invalid, validate
from native_build import build, verify_build
from receipt import canonical, policy_at, sha, snapshot
from service_graph import resolve


CORE = {"id", "role", "scope", "files", "public_headers", "dependencies"}
DETAIL = {"source_root", "responsibility", "non_responsibilities", "boundary", "languages", "exports", "targets",
          "linkage", "compositions", "activation_condition", "lifecycle", "provides", "requires", "third_party_dependencies", "budgets", "required_modules"}
LANGUAGES = {".c": "c17", ".h": "c-header", ".S": "gas"}
CONTROL_KINDS = {"control", "gate", "gate-test", "instructions"}
MISSING = ["independent control approval", "protected status publisher", "stage-specific ABI/runtime/performance gates"]


def fields(value, names, label):
    if not isinstance(value, dict) or set(value) != set(names):
        raise Invalid("invalid " + label + " fields")


def text(value, label):
    if not isinstance(value, str) or not value.strip():
        raise Invalid("missing " + label)


def strings(value, label):
    if not isinstance(value, list) or any(not isinstance(x, str) or not x.strip() for x in value) or len(set(value)) != len(value):
        raise Invalid("invalid " + label + " list")


def path_name(value):
    text(value, "relative path")
    path = PurePosixPath(value)
    if path.is_absolute() or str(path) != value or ".." in path.parts or "\\" in value or path.parts[0] in {".git", "references"}:
        raise Invalid("unsafe admission path")
    return path


def budget(value, metric):
    fields(value, {metric, "measurement_plan"}, "budget")
    text(value["measurement_plan"], "budget measurement plan")
    fields(value[metric], {"debug", "release"}, "mode budget")
    if any(v is not None and (type(v) is not int or v <= 0) for v in value[metric].values()):
        raise Invalid("budget must be unbaselined or a positive byte limit")


def registry(value, base_files):
    fields(value, {"schema", "phase", "target", "toolchain", "modules", "service_definitions", "builds"}, "admission registry")
    if value["schema"] != "pangu.native-admission.v1" or value["phase"] != "CANDIDATE_NOT_APPROVED" or value["target"] != "linux-x86_64":
        raise Invalid("unsupported admission schema, phase or target")
    if base_files.get(value["toolchain"]) != "control":
        raise Invalid("toolchain must be controller-owned control data")
    if any(not isinstance(value[k], list) for k in ("modules", "service_definitions", "builds")):
        raise Invalid("registry arrays required")
    for module in value["modules"]:
        fields(module, CORE | DETAIL, "module")
    manifest = {"schema": "pangu.modules.v1", "modules": [{k: m[k] for k in CORE} for m in value["modules"]]}
    modules, owners = validate(manifest)
    details = {m["id"]: m for m in value["modules"]}
    for name, m in details.items():
        if not re.fullmatch(r"[a-z][a-z0-9-]*", name):
            raise Invalid("invalid module identifier")
        root = path_name(m["source_root"])
        if root.parts[0] in {"docs", "tools", "governance"}:
            raise Invalid("native source root overlaps control directories")
        text(m["responsibility"], "module responsibility")
        strings(m["non_responsibilities"], "non-responsibilities")
        if not m["non_responsibilities"]:
            raise Invalid("explicit non-responsibilities required")
        fields(m["boundary"], {"reason", "evidence"}, "module boundary")
        if m["boundary"]["reason"] not in {"required-replacement-point", "multiple-consumers", "independent-lifecycle", "independent-delivery"}:
            raise Invalid("unregistered module boundary reason")
        strings(m["boundary"]["evidence"], "boundary evidence")
        if not m["boundary"]["evidence"] or any(base_files.get(p) not in {"document", "control", "instructions"} for p in m["boundary"]["evidence"]):
            raise Invalid("boundary requires registered review evidence")
        if any(not PurePosixPath(p).parent.is_relative_to(root) or p in base_files for p in m["files"]):
            raise Invalid("source ownership overlaps root or control file")
        if any(PurePosixPath(p).suffix not in LANGUAGES for p in m["files"]):
            raise Invalid("native admission language not implemented")
        strings(m["languages"], "module languages")
        if set(m["languages"]) != {LANGUAGES[PurePosixPath(p).suffix] for p in m["files"]}:
            raise Invalid("declared languages do not match compilation inputs")
        if m["role"] == "interface" and (m["languages"] != ["c-header"] or m["exports"] or m["provides"] or m["requires"]):
            raise Invalid("interface module cannot contain implementation")
        strings(m["exports"], "public symbols")
        if any(not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", s) for s in m["exports"]):
            raise Invalid("invalid public symbol")
        if m["targets"] != ["linux-x86_64"] or m["linkage"] != "static" or m["third_party_dependencies"] != []:
            raise Invalid("unsupported platform, linkage or third-party dependency")
        strings(m["compositions"], "module compositions")
        strings(m["required_modules"], "required modules")
        if not set(m["required_modules"]) <= set(m["dependencies"]):
            raise Invalid("required modules must be declared allowed dependencies")
        expected_activation = "definition-only" if m["role"] == "interface" else "profile-selected"
        if m["activation_condition"] != expected_activation:
            raise Invalid("unsupported activation condition")
        lifecycle = m["lifecycle"]
        fields(lifecycle, {"activate", "dispose", "state_owner", "threading", "callbacks", "resource_contract"}, "lifecycle")
        for key in ("state_owner", "resource_contract"):
            text(lifecycle[key], key)
        if lifecycle["threading"] not in {"single-thread", "external-synchronization"} or lifecycle["callbacks"] not in {"none", "non-reentrant", "explicit-reentrancy"}:
            raise Invalid("unsupported threading or callback contract")
        if any(lifecycle[k] is not None and lifecycle[k] not in m["exports"] for k in ("activate", "dispose")):
            raise Invalid("lifecycle entry must be a declared public symbol")
        budget(m["budgets"], "object_bytes")
        if m["role"] in {"host", "foundation", "platform"} and any(modules[d]["role"] in {"plugin", "client"} for d in m["dependencies"]):
            raise Invalid("low-level module depends on business or client")
    builds, services = {}, {}
    for item in value["builds"]:
        fields(item, {"id", "scope", "modules", "modes", "entry", "bindings", "budgets"}, "build")
        name = item["id"]
        if not isinstance(name, str) or not re.fullmatch(r"[a-z][a-z0-9-]*", name) or name in builds:
            raise Invalid("invalid or duplicate build identifier")
        strings(item["modules"], "selected modules")
        selected = set(item["modules"])
        if not selected or not selected <= set(modules) or item["modes"] != ["debug", "release"]:
            raise Invalid("unknown modules or incomplete required build modes")
        if any(not set(details[m]["required_modules"]) <= selected for m in selected):
            raise Invalid("selected build is not dependency-closed")
        if item["scope"] == "product" and any(modules[m]["scope"] != "product" for m in selected):
            raise Invalid("product build contains non-product module")
        fields(item["entry"], {"module", "symbol"}, "build entry")
        entry = item["entry"]
        if (entry["module"] not in selected or entry["symbol"] not in details[entry["module"]]["exports"]
                or modules[entry["module"]]["role"] not in {"host", "platform", "client"}):
            raise Invalid("entry must belong to selected bootstrap/platform/client")
        budget(item["budgets"], "artifact_bytes")
        composition = {"schema": "pangu.service-composition.v1", "services": value["service_definitions"],
                       "plugins": [{"module": m["id"], "provides": m["provides"], "requires": m["requires"]}
                                   for m in value["modules"] if m["role"] != "interface"],
                       "profile": {"id": name, "scope": item["scope"],
                                   "modules": [m for m in item["modules"] if modules[m]["role"] != "interface"], "bindings": item["bindings"]}}
        services[name] = resolve(manifest, composition)
        if services[name]["required_missing"]:
            raise Invalid("required service missing from build composition")
        builds[name] = item
    for name, module in details.items():
        expected = {b for b, item in builds.items() if name in item["modules"]}
        if not expected or set(module["compositions"]) != expected:
            raise Invalid("module has missing or inconsistent required build coverage")
    if bool(modules) != bool(builds) or (not modules and value["service_definitions"]):
        raise Invalid("incomplete native registry")
    return manifest, details, builds, services


def inventory(root, allowed):
    root, found = Path(root).resolve(strict=True), {}
    def failed(error):
        raise Invalid("cannot scan admission tree: " + str(error))
    for parent, dirs, files in os.walk(root, followlinks=False, onerror=failed):
        base = Path(parent)
        for name in list(dirs):
            path = base / name
            relative = path.relative_to(root).as_posix()
            if base == root and name in {".git", "references"}:
                dirs.remove(name)
            elif path.is_symlink() or not any(p.startswith(relative + "/") for p in allowed):
                raise Invalid("unknown or symlink admission directory: " + relative)
        for name in files:
            path = base / name
            relative = path.relative_to(root).as_posix()
            if base == root and name in {".git", "references"}:
                continue
            if relative not in allowed or path.is_symlink() or not path.is_file():
                raise Invalid("unknown or non-regular admission file: " + relative)
            found[relative] = {"sha256": sha(path.read_bytes()), "mode": stat.S_IMODE(path.stat().st_mode)}
    if set(found) != set(allowed):
        raise Invalid("registered admission file missing")
    return found


def preflight(candidate, controller, policy_pin, controller_pin, registry_path):
    candidate, controller = Path(candidate).resolve(strict=True), Path(controller).resolve(strict=True)
    policy = policy_at(controller, policy_pin)
    control = snapshot(controller, policy)
    if sha(canonical(control)) != controller_pin:
        raise Invalid("controller tree pin mismatch")
    path_name(registry_path)
    if policy["files"].get(registry_path) != "control":
        raise Invalid("registry is not controller-owned control data")
    proposal = json.loads((controller / registry_path).read_text())
    manifest, modules, builds, services = registry(proposal, policy["files"])
    allowed = set(policy["files"]) | {p for m in modules.values() for p in m["files"]}
    before = inventory(candidate, allowed)
    for name, kind in policy["files"].items():
        if kind in CONTROL_KINDS and before[name] != control[name]:
            raise Invalid("candidate changed controller-owned file: " + name)
    return {"policy": policy, "control": control, "proposal": proposal, "manifest": manifest,
            "modules": modules, "builds": builds, "services": services, "inputs": before, "allowed": allowed}


def project(selected):
    return ({"schema": "pangu.modules.v1", "modules": [
        {**{k: m[k] for k in CORE}, "dependencies": [d for d in m["dependencies"] if d in selected]}
        for m in selected.values()]}, {name: m["exports"] for name, m in selected.items()})


def stage(selected, candidate, before, staging):
    for m in selected.values():
        for name in m["files"]:
            destination = staging / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(candidate / name, destination)
            if sha(destination.read_bytes()) != before[name]["sha256"]:
                raise Invalid("native source changed during staging")


def execute(candidate, controller, policy_pin, controller_pin, registry_path, output):
    candidate, controller, output = Path(candidate).resolve(strict=True), Path(controller).resolve(strict=True), Path(output).absolute()
    if any(output.resolve().is_relative_to(p) for p in (candidate, controller)) or output.exists() or output.is_symlink():
        raise Invalid("output must be new and outside input trees")
    checked = preflight(candidate, controller, policy_pin, controller_pin, registry_path)
    policy, control, proposal = checked["policy"], checked["control"], checked["proposal"]
    modules, builds, services = checked["modules"], checked["builds"], checked["services"]
    before, allowed = checked["inputs"], checked["allowed"]
    report = {"schema": "pangu.native-admission-run.v1", "status": "BLOCKED", "release": "BLOCKED",
              "registry_sha256": sha(canonical(proposal)), "policy_sha256": policy_pin, "controller_sha256": controller_pin,
              "inputs": before, "candidate_tree_sha256": sha(canonical(before)), "builds": [], "service_preflight": services,
              "native_matrix": "NOT_RUN", "unbaselined_budgets": [],
              "execution_environment": "CALLER_PROVIDED_NOT_ATTESTED",
              "missing": list(MISSING)}
    if not builds:
        report["missing"].append("registered native modules and mandatory builds")
    else:
        output.mkdir()
        profile = json.loads((controller / proposal["toolchain"]).read_text())
        profile_pin = sha(canonical(profile))
        for build_id, item in builds.items():
            selected = {m: modules[m] for m in item["modules"]}
            subset, exports = project(selected)
            with tempfile.TemporaryDirectory(prefix="pangu-admission-inputs-") as temporary:
                staging = Path(temporary)
                stage(selected, candidate, before, staging)
                for mode in item["modes"]:
                    destination = output / (build_id + "-" + mode)
                    native = build(staging, subset, exports, profile, profile_pin, mode, item["entry"]["symbol"], destination)
                    receipt_pin = sha((destination / "build.json").read_bytes())
                    checked = verify_build(staging, subset, exports, profile, profile_pin, mode, item["entry"]["symbol"], destination, receipt_pin)
                    object_names = native["link_evidence"]["final_build"]["retained_objects"]
                    sizes = {name: sum((destination / artifact).stat().st_size for source, artifact in object_names.items()
                                       if source in m["files"]) for name, m in selected.items()}
                    for name, m in selected.items():
                        limit = m["budgets"]["object_bytes"][mode]
                        if limit is None:
                            report["unbaselined_budgets"].append({"module": name, "mode": mode, "metric": "object_bytes"})
                        elif sizes[name] > limit:
                            raise Invalid("module object byte budget exceeded: " + name)
                    artifact_size = (destination / "probe.elf").stat().st_size
                    limit = item["budgets"]["artifact_bytes"][mode]
                    if limit is None:
                        report["unbaselined_budgets"].append({"build": build_id, "mode": mode, "metric": "artifact_bytes"})
                    elif artifact_size > limit:
                        raise Invalid("final artifact byte budget exceeded: " + build_id)
                    report["builds"].append({"id": build_id, "mode": mode, "build_receipt_sha256": receipt_pin,
                                             "artifact_binding": checked, "object_bytes": sizes, "artifact_bytes": artifact_size})
        report["native_matrix"] = "PASS"
    if inventory(candidate, allowed) != before or snapshot(controller, policy) != control:
        raise Invalid("admission inputs changed during build")
    if not output.exists():
        output.mkdir()
    (output / "admission.json").write_bytes(canonical(report) + b"\n")
    return report


def verify(candidate, controller, policy_pin, controller_pin, registry_path, output, receipt_pin):
    candidate, controller, output = Path(candidate).resolve(strict=True), Path(controller).resolve(strict=True), Path(output).absolute()
    if output.is_symlink() or any(output.resolve().is_relative_to(p) for p in (candidate, controller)):
        raise Invalid("invalid admission receipt directory")
    path = output / "admission.json"
    if path.is_symlink() or sha(path.read_bytes()) != receipt_pin:
        raise Invalid("admission receipt pin mismatch")
    previous = json.loads(path.read_text())
    context = preflight(candidate, controller, policy_pin, controller_pin, registry_path)
    inputs, builds, modules = context["inputs"], context["builds"], context["modules"]
    if (previous["schema"] != "pangu.native-admission-run.v1" or previous["status"] != "BLOCKED" or previous["release"] != "BLOCKED"
            or previous["policy_sha256"] != policy_pin or previous["controller_sha256"] != controller_pin
            or previous["registry_sha256"] != sha(canonical(context["proposal"]))
            or previous["inputs"] != inputs or previous["candidate_tree_sha256"] != sha(canonical(inputs))
            or previous["service_preflight"] != context["services"]
            or previous["execution_environment"] != "CALLER_PROVIDED_NOT_ATTESTED"
            or previous["missing"] != MISSING + ([] if builds else ["registered native modules and mandatory builds"])):
        raise Invalid("admission receipt context or source changed")
    expected = [(name, mode) for name, item in builds.items() for mode in item["modes"]]
    if ([(r["id"], r["mode"]) for r in previous["builds"]] != expected
            or previous["native_matrix"] != ("PASS" if builds else "NOT_RUN")):
        raise Invalid("admission matrix coverage mismatch")
    if {p.name for p in output.iterdir()} != {"admission.json"} | {name + "-" + mode for name, mode in expected}:
        raise Invalid("admission artifact inventory mismatch")
    profile = json.loads((controller / context["proposal"]["toolchain"]).read_text())
    profile_pin = sha(canonical(profile))
    unbaselined = []
    for record in previous["builds"]:
        item, mode = builds[record["id"]], record["mode"]
        selected = {m: modules[m] for m in item["modules"]}
        subset, exports = project(selected)
        destination = output / (record["id"] + "-" + mode)
        if destination.is_symlink() or not destination.is_dir():
            raise Invalid("invalid retained build directory")
        with tempfile.TemporaryDirectory(prefix="pangu-admission-recheck-") as temporary:
            staging = Path(temporary)
            stage(selected, candidate, inputs, staging)
            result = verify_build(staging, subset, exports, profile, profile_pin, mode, item["entry"]["symbol"],
                                  destination, record["build_receipt_sha256"])
            if result != record["artifact_binding"]:
                raise Invalid("retained build verification mismatch")
        native = json.loads((destination / "build.json").read_text())
        objects = native["link_evidence"]["final_build"]["retained_objects"]
        actual = {name: sum((destination / artifact).stat().st_size for source, artifact in objects.items()
                            if source in m["files"]) for name, m in selected.items()}
        size = (destination / "probe.elf").stat().st_size
        if (record["object_bytes"] != actual or any(type(v) is not int for v in record["object_bytes"].values())
                or type(record["artifact_bytes"]) is not int or record["artifact_bytes"] != size):
            raise Invalid("admission measurements do not match artifacts")
        for name, m in selected.items():
            limit = m["budgets"]["object_bytes"][mode]
            if limit is None:
                unbaselined.append({"module": name, "mode": mode, "metric": "object_bytes"})
            elif actual[name] > limit:
                raise Invalid("retained module object budget exceeded")
        limit = item["budgets"]["artifact_bytes"][mode]
        if limit is None:
            unbaselined.append({"build": item["id"], "mode": mode, "metric": "artifact_bytes"})
        elif size > limit:
            raise Invalid("retained final artifact budget exceeded")
    if previous["unbaselined_budgets"] != unbaselined:
        raise Invalid("unbaselined budgets omitted or forged")
    if preflight(candidate, controller, policy_pin, controller_pin, registry_path) != context:
        raise Invalid("admission inputs changed during verification")
    return {"admission_receipt_binding": "PASS", "verified_builds": len(expected),
            "native_matrix": previous["native_matrix"], "status": "BLOCKED", "release": "BLOCKED"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("candidate", "controller", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--policy-sha256", required=True)
    parser.add_argument("--controller-sha256", required=True)
    parser.add_argument("--registry", default="governance/native-admission.json")
    parser.add_argument("--verify-receipt-sha256", help="recheck a retained matrix instead of building")
    args = parser.parse_args()
    try:
        if args.controller.resolve(strict=True) != Path(__file__).resolve().parents[2]:
            raise Invalid("execute admission.py from the designated controller")
        arguments = (args.candidate, args.controller, args.policy_sha256, args.controller_sha256, args.registry, args.output)
        if args.verify_receipt_sha256:
            print(json.dumps(verify(*arguments, args.verify_receipt_sha256)))
            return 2
        report = execute(*arguments)
    except (Invalid, ValueError, TypeError, KeyError, OSError, subprocess.SubprocessError) as error:
        print("native admission FAIL:", error)
        return 1
    print(json.dumps({"status": report["status"], "native_matrix": report["native_matrix"], "builds": len(report["builds"]),
                      "receipt": str(args.output / "admission.json"), "receipt_sha256": sha((args.output / "admission.json").read_bytes())}))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
