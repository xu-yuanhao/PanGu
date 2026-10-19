"""Freeze local development tools and run controller tests in a read-only namespace.

This is a local, test-only image. Its digest must be approved externally before use
as a production trust root; it does not establish GitHub credential separation.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess

from compile_inputs import digest
from native_build import validate_native_profile
from receipt import canonical, capture, sha


def dependencies(text):
    result = set()
    for line in text.splitlines():
        value = line.strip()
        if not value or value.startswith("linux-vdso.so.") or value == "statically linked":
            continue
        match = re.fullmatch(r"(?:\S+ => )?(/\S+) \(0x[0-9a-fA-F]+\)", value)
        if not match:
            raise ValueError("unrecognized or unresolved loader dependency: " + value)
        result.add(Path(match[1]))
    return result


def freeze(profile, profile_pin, directory):
    directory = Path(directory).absolute()
    repository = Path(__file__).resolve().parents[2]
    if directory.resolve().is_relative_to(repository):
        raise ValueError("tool image must be outside repository")
    if sha(canonical(profile)) != profile_pin:
        raise ValueError("tool image profile pin mismatch")
    validate_native_profile(profile)
    directory.mkdir()
    rootfs = directory / "rootfs"
    rootfs.mkdir()
    files, queued = {}, set()

    def add(path, inspect_dependencies=False):
        path = Path(path).absolute()
        if not path.is_file() or not path.resolve().is_relative_to("/usr"):
            raise ValueError("tool image input outside /usr or not a file: " + str(path))
        # Dereference system-directory aliases while preserving requested SONAMEs.
        name = (path.parent.resolve() / path.name).as_posix()
        if name in files:
            return
        target = rootfs / name.lstrip("/")
        target.parent.mkdir(parents=True, exist_ok=True)
        before = digest(path)
        shutil.copyfile(path, target)
        target.chmod(stat.S_IMODE(path.stat().st_mode))
        if digest(target) != before or digest(path) != before:
            raise ValueError("tool image input changed while copying")
        files[name] = {"source": str(path), "sha256": before, "mode": stat.S_IMODE(target.stat().st_mode)}
        if inspect_dependencies:
            queued.add(path)

    for spec in profile["tools"].values():
        add(spec["path"], True)
    for command in ("clang", "python3", "git"):
        path = shutil.which(command)
        if not path:
            raise ValueError("tool image required binary missing: " + command)
        add(path, True)
    # Isolated Python needs its standard library and extension modules, not pip packages.
    standard = Path(os.__file__).resolve().parent
    trees = [standard, Path("/usr/share/gdb"), Path("/usr/lib/gconv"), Path("/usr/share/guile/3.0"),
             Path("/usr/lib/guile/3.0/ccache"), Path("/usr/share/git-core/templates")]
    for tree in trees:
        if not tree.is_dir():
            raise ValueError("tool runtime data missing: " + str(tree))
        for parent, dirs, names in os.walk(tree, followlinks=False):
            dirs[:] = [d for d in dirs if d not in {"site-packages", "__pycache__", "tkinter", "idlelib", "turtledemo"}]
            for child in dirs:
                if (Path(parent) / child).is_symlink():
                    raise ValueError("unresolved runtime data directory symlink")
            for name in names:
                if name.startswith("_tkinter.") or name == "turtle.py":
                    continue
                path = Path(parent) / name
                add(path, path.suffix == ".so")
    inspected = set()
    while queued - inspected:
        path = sorted(queued - inspected)[0]
        inspected.add(path)
        result = subprocess.run(["/usr/lib/ld-linux-x86-64.so.2", "--list", str(path)], capture_output=True, text=True, timeout=15,
                                env={"PATH": "/usr/bin:/bin", "LC_ALL": "C"})
        if result.returncode or result.stderr:
            raise ValueError("loader dependency inspection failed: " + result.stderr)
        for dependency in dependencies(result.stdout):
            add(dependency, True)
    for name, item in files.items():
        if digest(Path(item["source"])) != item["sha256"]:
            raise ValueError("tool runtime changed during image freeze")
    bwrap = Path(shutil.which("bwrap") or "/missing-bwrap").resolve(strict=True)
    report = {"schema": "pangu.local-tool-image.v1", "status": "CANDIDATE_NOT_APPROVED",
              "profile_sha256": profile_pin, "files": files, "file_count": len(files),
              "total_bytes": sum((rootfs / n.lstrip("/")).stat().st_size for n in files),
              "loader_inspector": {"path": "/usr/lib/ld-linux-x86-64.so.2", "sha256": digest(Path("/usr/lib/ld-linux-x86-64.so.2"))},
              "bwrap": {"path": str(bwrap), "sha256": digest(bwrap)},
              "scope": "development-tools-only", "production_runtime_dependencies": "NOT_ADDED"}
    report["excluded_optional_python_gui"] = ["_tkinter", "tkinter", "idlelib", "turtledemo", "turtle"]
    (directory / "image.json").write_bytes(canonical(report) + b"\n")
    verify(directory, sha((directory / "image.json").read_bytes()))
    return report


def verify(directory, manifest_pin):
    directory = Path(directory).resolve(strict=True)
    manifest = directory / "image.json"
    if manifest.is_symlink() or digest(manifest) != manifest_pin:
        raise ValueError("tool image manifest pin mismatch")
    report = json.loads(manifest.read_text())
    if report["schema"] != "pangu.local-tool-image.v1" or report["status"] != "CANDIDATE_NOT_APPROVED":
        raise ValueError("unsupported tool image manifest")
    if not isinstance(report["files"], dict) or not report["files"]:
        raise ValueError("empty tool image manifest")
    rootfs = directory / "rootfs"
    if rootfs.is_symlink() or not rootfs.is_dir():
        raise ValueError("invalid tool image root")
    found = set()
    for parent, dirs, names in os.walk(rootfs, followlinks=False):
        for name in dirs + names:
            path = Path(parent) / name
            if path.is_symlink():
                raise ValueError("tool image symlink forbidden")
        for name in names:
            path = Path(parent) / name
            relative = "/" + path.relative_to(rootfs).as_posix()
            if relative not in report["files"]:
                raise ValueError("unknown tool image file")
            item = report["files"][relative]
            if not path.is_file() or digest(path) != item["sha256"] or stat.S_IMODE(path.stat().st_mode) != item["mode"]:
                raise ValueError("tool image file changed")
            found.add(relative)
    if found != set(report["files"]) or len(found) != report["file_count"]:
        raise ValueError("tool image file missing")
    if digest(Path(report["bwrap"]["path"])) != report["bwrap"]["sha256"]:
        raise ValueError("namespace launcher changed")
    return report


def run_image(directory, manifest_pin, controller, candidate, policy_pin, controller_pin, output,
              job="self-tests", registry="governance/native-admission.json"):
    report = verify(directory, manifest_pin)
    controller, candidate, output = Path(controller).resolve(strict=True), Path(candidate).resolve(strict=True), Path(output).absolute()
    if any(output.resolve().is_relative_to(root) for root in (controller, candidate, Path(directory).resolve())):
        raise ValueError("isolated output must be outside inputs")
    if job == "self-tests":
        collect = lambda: capture(candidate, controller, policy_pin)
        before = collect()
        if sha(canonical(before["trusted_files"])) != controller_pin:
            raise ValueError("controller tree pin mismatch")
        script = "runner.py"
        child_arguments = ["--candidate", "/work/candidate", "--trusted-root", "/work/controller", "--policy-sha256", policy_pin,
                           "--output", "/evidence/run.json"]
        result_path = output / "run.json"
    elif job == "native-admission":
        from admission import preflight
        collect = lambda: preflight(candidate, controller, policy_pin, controller_pin, registry)
        before = collect()
        toolchain = json.loads((controller / before["proposal"]["toolchain"]).read_text())
        if sha(canonical(toolchain)) != report["profile_sha256"]:
            raise ValueError("native job toolchain does not match frozen image")
        script = "admission.py"
        child_arguments = ["--candidate", "/work/candidate", "--controller", "/work/controller", "--policy-sha256", policy_pin,
                           "--controller-sha256", controller_pin, "--registry", registry, "--output", "/evidence/native"]
        result_path = output / "native/admission.json"
    else:
        raise ValueError("unknown isolated job")
    output.mkdir()
    rootfs = Path(directory).resolve() / "rootfs"
    bootstrap = "import runpy,sys;sys.path.insert(0,'/work/controller/tools/gates');sys.argv=['/work/controller/tools/gates/" + script + "']+sys.argv[1:];runpy.run_path(sys.argv[0],run_name='__main__')"
    command = [report["bwrap"]["path"], "--unshare-all", "--die-with-parent", "--new-session", "--clearenv",
               "--ro-bind", str(rootfs / "usr"), "/usr", "--symlink", "usr/lib", "/lib", "--symlink", "usr/lib", "/lib64",
               "--symlink", "usr/bin", "/bin", "--proc", "/proc", "--dev", "/dev", "--tmpfs", "/tmp",
               "--ro-bind", str(controller), "/work/controller", "--ro-bind", str(candidate), "/work/candidate",
               "--bind", str(output), "/evidence", "--chdir", "/work/controller",
               "--setenv", "PATH", "/usr/bin:/bin", "--setenv", "LC_ALL", "C", "--",
               "/usr/bin/python3", "-I", "-S", "-B", "-c", bootstrap] + child_arguments
    result = subprocess.run(command, capture_output=True, text=True, timeout=180)
    (output / "stdout.log").write_text(result.stdout)
    (output / "stderr.log").write_text(result.stderr)
    verify(directory, manifest_pin)
    if collect() != before:
        raise ValueError("isolated controller inputs changed")
    summary = {"schema": "pangu.isolated-controller-run.v1", "status": "BLOCKED", "image_manifest_sha256": manifest_pin,
               "command": command, "returncode": result.returncode, "network": "UNSHARED", "inputs": "READ_ONLY_BINDS",
               "host_home": "NOT_MOUNTED", "kernel": "HOST_KERNEL", "external_trust_approval": "PENDING", "release": "BLOCKED"}
    if result.returncode != 2 or not result_path.is_file():
        raise ValueError("isolated controller failed; inspect " + str(output / "stderr.log") + " and stdout.log")
    payload = json.loads(result_path.read_text())
    summary["run_sha256"] = digest(result_path)
    summary["job"] = job
    summary["test_count"] = payload.get("test_count") if job == "self-tests" else None
    if job == "native-admission":
        summary["native_matrix"] = payload["native_matrix"]
        summary["build_count"] = len(payload["builds"])
    (output / "isolation.json").write_bytes(canonical(summary) + b"\n")
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("freeze", "verify", "run"))
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--image-sha256")
    parser.add_argument("--toolchain", type=Path)
    parser.add_argument("--toolchain-sha256")
    parser.add_argument("--controller", type=Path)
    parser.add_argument("--candidate", type=Path)
    parser.add_argument("--policy-sha256")
    parser.add_argument("--controller-sha256")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--job", choices=("self-tests", "native-admission"), default="self-tests")
    parser.add_argument("--registry", default="governance/native-admission.json")
    args = parser.parse_args()
    try:
        if args.mode == "freeze":
            if not args.toolchain or not args.toolchain_sha256:
                raise ValueError("freeze requires toolchain and external profile pin")
            report = freeze(json.loads(args.toolchain.read_text()), args.toolchain_sha256, args.image)
            print(json.dumps({"status": report["status"], "files": report["file_count"], "bytes": report["total_bytes"],
                              "image_sha256": digest(args.image / "image.json")}))
        elif args.mode == "verify":
            verify(args.image, args.image_sha256)
            print(json.dumps({"tool_image_binding": "PASS", "status": "BLOCKED"}))
        else:
            if not all((args.controller, args.candidate, args.policy_sha256, args.controller_sha256, args.output)):
                raise ValueError("run requires controller, candidate, policy pin and output")
            report = run_image(args.image, args.image_sha256, args.controller, args.candidate, args.policy_sha256,
                               args.controller_sha256, args.output, args.job, args.registry)
            print(json.dumps({"status": report["status"], "job": report["job"], "test_count": report["test_count"],
                              "build_count": report.get("build_count"), "isolation_receipt": str(args.output / "isolation.json")}))
    except (ValueError, TypeError, KeyError, OSError, subprocess.SubprocessError) as error:
        print("tool image FAIL:", error)
        return 1
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
