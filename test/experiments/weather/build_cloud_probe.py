"""Build the cloud consumer against the production native GPU platform selection."""

import argparse
import os
import platform
import shlex
import subprocess
from pathlib import Path
from runpy import run_path
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[3]


def build(output):
    system = platform.system()
    if system not in ("Linux", "Darwin"):
        raise RuntimeError("the native cloud probe requires Linux or Darwin")
    select = run_path(ROOT / "test/scripts/source_units.py")["source_units"]
    native = ROOT / "src/render/device/native"
    units = [unit for unit in select(ROOT) if unit.is_relative_to(native)]
    if not units:
        raise RuntimeError("no production native GPU implementation is available")
    units += [
        ROOT / "src/world/weather/CloudShell.cpp",
        ROOT / "test/experiments/weather/cloud_native_probe.cpp",
    ]
    packages = ["spirv-cross-c-shared"] + (["vulkan"] if system == "Linux" else [])
    includes = subprocess.check_output(["pkg-config", "--cflags", *packages], text=True)
    libraries = shlex.split(
        subprocess.check_output(["pkg-config", "--libs", *packages], text=True)
    )
    if system == "Darwin":
        libraries += ["-framework", "Foundation", "-framework", "Metal"]
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    flags = [
        "-std=c++23",
        "-O2",
        "-ffp-contract=off",
        "-fno-exceptions",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Wshadow",
        "-Werror",
        *shlex.split(includes),
    ]
    flags += [
        "-I" + str(ROOT / path)
        for path in (
            "include",
            "src/world/weather",
            "src/render/stages",
            "src/render/device",
        )
    ]
    output.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="outshine-cloud-build-") as scratch:
        objects = []
        for index, unit in enumerate(units):
            obj = str(Path(scratch) / f"{index}.o")
            arc = ["-fobjc-arc"] if unit.suffix == ".mm" else []
            subprocess.run(
                [*compiler, *flags, *arc, "-c", str(unit), "-o", obj],
                cwd=ROOT,
                check=True,
            )
            objects.append(obj)
        subprocess.run(
            [*compiler, *objects, *libraries, "-pthread", "-o", str(output)],
            cwd=ROOT,
            check=True,
        )
    print(f"built {output} using {len(units)} production/consumer units on {system}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.output.resolve())


if __name__ == "__main__":
    main()
