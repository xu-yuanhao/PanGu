#!/usr/bin/env python3
"""Build and test one component, or the integrated product, through CMake/CTest."""

import argparse
import json
from pathlib import Path
import subprocess
import sys


COMPONENTS = ("core", "agent", "services", "tui")


def run(command, capture=False):
    print("+ " + " ".join(str(part) for part in command), flush=True)
    return subprocess.run(command, check=True, text=True,
                          stdout=subprocess.PIPE if capture else None)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scope", choices=(*COMPONENTS, "all"))
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--parallel", type=int, default=2)
    parser.add_argument("--config", default="Debug")
    args = parser.parse_args()
    if args.parallel < 1:
        parser.error("--parallel must be positive")

    root = Path(__file__).resolve().parents[2]
    if not (root / "CMakeLists.txt").is_file():
        print("NOT RUN: root CMake build has not been implemented (C01).", file=sys.stderr)
        return 2
    build = (args.build_dir or root / "build" / args.scope).resolve()
    if not (build / "CMakeCache.txt").is_file():
        run(["cmake", "-S", str(root), "-B", str(build), "-DBUILD_TESTING=ON",
             "-DCMAKE_BUILD_TYPE=" + args.config,
             "-DPANGU_BUILD_TUI=" + ("ON" if args.scope in ("tui", "all") else "OFF"),
             "-DPANGU_BUILD_SERVICES=" + ("ON" if args.scope in ("services", "agent", "all") else "OFF"),
             "-DPANGU_BUILD_AGENT=" + ("ON" if args.scope in ("agent", "all") else "OFF")])

    build_command = ["cmake", "--build", str(build), "--config", args.config,
                     "--parallel", str(args.parallel)]
    if args.scope != "all":
        build_command += ["--target", "pangu_check_" + args.scope]
    run(build_command)

    command = ["ctest", "--test-dir", str(build), "-C", args.config]
    if args.scope != "all":
        command += ["-L", "^" + args.scope + "$"]
    listing = json.loads(run(command + ["--show-only=json-v1"], capture=True).stdout)
    tests = listing.get("tests", [])
    if not tests:
        print("NOT RUN: no tests registered for " + args.scope, file=sys.stderr)
        return 2
    if args.scope == "all":
        labels = {label for test in tests for prop in test.get("properties", [])
                  if prop["name"] == "LABELS" for label in prop["value"]}
        missing = set((*COMPONENTS, "integration")) - labels
        if missing:
            print("NOT RUN: integrated test groups missing: " + ", ".join(sorted(missing)),
                  file=sys.stderr)
            return 2

    print("Selected tests: " + str(len(tests)), flush=True)
    run(command + ["--output-on-failure", "--no-tests=error",
                   "--parallel", str(args.parallel)])
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print("Check failed: " + str(error), file=sys.stderr)
        sys.exit(2)
