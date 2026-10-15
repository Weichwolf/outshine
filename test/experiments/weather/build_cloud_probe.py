"""Link the cloud consumer against the regular production engine archive."""

import argparse
import os
import shlex
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[3]


def build(output, archive):
    if not archive.is_file():
        raise FileNotFoundError(
            "build the matching production archive with make all first"
        )
    units = [
        ROOT / "src/world/weather/CloudShell.cpp",
        ROOT / "test/experiments/weather/cloud_native_probe.cpp",
    ]
    libraries = shlex.split(
        subprocess.check_output(
            ["sh", "test/run.sh", "--native-link-flags"], cwd=ROOT, text=True
        )
    )
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
            subprocess.run(
                [*compiler, *flags, "-c", str(unit), "-o", obj],
                cwd=ROOT,
                check=True,
            )
            objects.append(obj)
        subprocess.run(
            [
                *compiler,
                *objects,
                str(archive),
                *libraries,
                "-pthread",
                "-o",
                str(output),
            ],
            cwd=ROOT,
            check=True,
        )
    print(f"built {output} from {len(units)} consumer units and {archive}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--archive", type=Path, default=ROOT / "build/liboutshine.a")
    args = parser.parse_args()
    build(args.output.resolve(), args.archive.resolve())


if __name__ == "__main__":
    main()
