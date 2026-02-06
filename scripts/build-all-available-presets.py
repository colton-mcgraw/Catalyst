#!/usr/bin/env python3
import argparse
import json
import os
import platform
import re
import subprocess
import sys
from pathlib import Path


PRESET_LINE_RE = re.compile(r'^\s+"(?P<name>[^"]+)"\s+-\s+')


def run(cmd: list[str]) -> int:
    print("==>", " ".join(cmd))
    proc = subprocess.run(cmd)
    return proc.returncode


def parse_listed_presets(output: str) -> list[str]:
    names: list[str] = []
    for line in output.splitlines():
        match = PRESET_LINE_RE.match(line)
        if match:
            names.append(match.group("name"))
    return names


def list_presets(cmd: list[str]) -> list[str]:
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout)
        raise RuntimeError(f"Command failed ({proc.returncode}): {' '.join(cmd)}")
    return sorted(parse_listed_presets(proc.stdout))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Configure/build all available CMake presets on this host (and optionally run all CTest presets)."
    )
    parser.add_argument("--run-tests", action="store_true", help="Also run all available CTest presets")
    parser.add_argument(
        "--require-os",
        choices=["Linux", "Darwin"],
        help="Fail if not running on this OS (useful for OS-specific wrappers)",
    )
    args = parser.parse_args()

    host = platform.system()
    if args.require_os and host != args.require_os:
        sys.stderr.write(f"This script is intended to run on {args.require_os} (host={host}).\n")
        return 2

    root = Path(__file__).resolve().parent.parent
    (root / "build").mkdir(parents=True, exist_ok=True)
    presets_path = root / "CMakePresets.json"
    if not presets_path.exists():
        sys.stderr.write(f"CMakePresets.json not found at: {presets_path}\n")
        return 1

    with presets_path.open("r", encoding="utf-8") as f:
        presets = json.load(f)

    build_to_configure: dict[str, str] = {}
    for bp in presets.get("buildPresets", []) or []:
        name = bp.get("name")
        cfg = bp.get("configurePreset")
        if name and cfg:
            build_to_configure[name] = cfg

    os.chdir(root)

    try:
        build_presets = list_presets(["cmake", "--build", "--list-presets"])
    except Exception as exc:
        sys.stderr.write(str(exc) + "\n")
        return 1

    if not build_presets:
        sys.stderr.write("No build presets were discovered.\n")
        return 1

    configured: set[str] = set()
    failed_configure: set[str] = set()
    failures: list[str] = []

    for build_preset in build_presets:
        cfg_preset = build_to_configure.get(build_preset)
        if not cfg_preset:
            failures.append(f"Missing configurePreset for build preset: {build_preset}")
            continue

        if cfg_preset in failed_configure:
            print(f"==> Skip build (configure failed): {build_preset}")
            continue

        if cfg_preset not in configured:
            rc = run(["cmake", "--preset", cfg_preset])
            if rc != 0:
                failures.append(f"Configure failed: {cfg_preset} (exit {rc})")
                failed_configure.add(cfg_preset)
                continue
            configured.add(cfg_preset)

        rc = run(["cmake", "--build", "--preset", build_preset])
        if rc != 0:
            failures.append(f"Build failed: {build_preset} (exit {rc})")

    if args.run_tests:
        try:
            test_presets = list_presets(["ctest", "--list-presets"])
        except Exception as exc:
            failures.append(f"Failed to list test presets: {exc}")
            test_presets = []

        for test_preset in test_presets:
            rc = run(["ctest", "--preset", test_preset])
            if rc != 0:
                failures.append(f"Tests failed: {test_preset} (exit {rc})")

    if failures:
        sys.stderr.write("\nFailures:\n")
        for item in failures:
            sys.stderr.write(f"- {item}\n")
        return 1

    print("All available preset builds completed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
