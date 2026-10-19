"""Pinned C17 probe entry point. Candidate profile, not a production toolchain."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import tempfile

from compile_inputs import C_FLAGS, audit, digest
from module_graph import Invalid


ENVIRONMENT = {"PATH": "/usr/bin:/bin", "LC_ALL": "C"}


def output(command):
    with tempfile.TemporaryDirectory(prefix="pangu-tool-identity-") as cache:
        result = subprocess.run(command, env={**ENVIRONMENT, "XDG_CACHE_HOME": cache}, capture_output=True, text=True, timeout=15)
    if result.returncode or result.stderr.strip():
        raise Invalid("tool identity probe failed: " + result.stderr)
    return result.stdout.strip()


def validate_profile(profile):
    if not isinstance(profile, dict) or set(profile) != {"schema", "status", "host", "compiler_target", "compile_flags", "link_flags", "tools"}:
        raise Invalid("unknown or missing toolchain fields")
    if profile["schema"] != "pangu.c17-probe-toolchain.v1" or profile["status"] != "CANDIDATE_NOT_APPROVED":
        raise Invalid("unsupported toolchain profile or approval claim")
    if profile["host"] != {"system": "Linux", "machine": "x86_64"}:
        raise Invalid("unsupported toolchain host")
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise Invalid("toolchain host mismatch")
    if profile["compiler_target"] != "x86_64-pc-linux-gnu":
        raise Invalid("unsupported compiler target")
    if profile["compile_flags"] != C_FLAGS or profile["link_flags"] != ["--fatal-warnings", "-r"]:
        raise Invalid("unapproved probe flags")
    tools = profile["tools"]
    if not isinstance(tools, dict) or set(tools) != {"compiler", "nm", "linker"}:
        raise Invalid("incomplete toolchain")
    for name, spec in tools.items():
        if not isinstance(spec, dict) or set(spec) != {"path", "sha256", "version"}:
            raise Invalid("invalid tool specification")
        if any(not isinstance(spec[k], str) or not spec[k] for k in spec):
            raise Invalid("invalid tool identity")
        path = Path(spec["path"])
        if not path.is_absolute() or path.resolve(strict=True) != path:
            raise Invalid("tool must use canonical absolute path")
        if digest(path) != spec["sha256"]:
            raise Invalid(name + " digest mismatch")
        if output([str(path), "--version"]) != spec["version"]:
            raise Invalid(name + " version mismatch")
    if output([tools["compiler"]["path"], "-dumpmachine"]) != profile["compiler_target"]:
        raise Invalid("compiler target mismatch")


def execute(root, manifest, exports, profile, expected_sha256):
    encoded = json.dumps(profile, sort_keys=True, separators=(",", ":")).encode()
    if hashlib.sha256(encoded).hexdigest() != expected_sha256:
        raise Invalid("toolchain profile pin mismatch")
    validate_profile(profile)
    tools = profile["tools"]
    probe = {"exports": exports}
    for name in ("nm", "linker"):
        probe[name] = tools[name]["path"]
        probe[name + "_sha256"] = tools[name]["sha256"]
    report = audit(root, manifest, tools["compiler"]["path"], tools["compiler"]["sha256"], probe)
    validate_profile(profile)
    report["toolchain_profile_sha256"] = expected_sha256
    report["toolchain_identity_audit"] = "PASS"
    report["toolchain_profile"] = profile
    report["environment"] = ENVIRONMENT
    report["missing"].extend(["pinned system loader and shared tool libraries", "hermetic toolchain image"])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--exports", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--toolchain-sha256", required=True, help="SHA-256 of canonical JSON, sorted keys and compact separators")
    args = parser.parse_args()
    try:
        report = execute(args.root, json.loads(args.manifest.read_text()), json.loads(args.exports.read_text()),
                         json.loads(args.toolchain.read_text()), args.toolchain_sha256)
    except (Invalid, ValueError, OSError, TypeError, KeyError, subprocess.SubprocessError) as error:
        print("toolchain FAIL:", error)
        return 1
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
