"""Compare native full-depth source curves without a production adoption (#353)."""

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

from cloud_air_transfer_native import compile_kernel, run_capture
from cloud_sun_reuse_native import ROOT, build_consumer, digest

PROFILES = {
    "legacy": [],
    "linear-512": ["-DSOURCE_LINEAR_DEPTH=1", "-DSOURCE_DEPTH_INTERVALS=512"],
    "linear-1024": ["-DSOURCE_LINEAR_DEPTH=1", "-DSOURCE_DEPTH_INTERVALS=1024"],
    "growth-64": [
        "-DSOURCE_LINEAR_DEPTH=1",
        "-DSOURCE_DEPTH_INTERVALS=1024",
        "-DSOURCE_DEPTH_GROWTH=0.015625",
    ],
    "growth-128": [
        "-DSOURCE_LINEAR_DEPTH=1",
        "-DSOURCE_DEPTH_INTERVALS=1024",
        "-DSOURCE_DEPTH_GROWTH=0.0078125",
    ],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--api-root", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--archive-receipt", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=3)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT) or not 1 <= args.repeats <= 16:
        parser.error("use an explicit temp output and1..16 warm repeats")
    args.output.mkdir(parents=True, exist_ok=True)
    record = {"commands": [], "profiles": {}}
    receipt = args.output / "receipt.json"

    def execute(command, folder, name, cwd=ROOT):
        command = list(map(str, command))
        start = time.monotonic()
        result = subprocess.run(
            command, cwd=cwd, capture_output=True, text=True, check=False
        )
        (folder / f"{name}.stdout").write_text(result.stdout)
        (folder / f"{name}.stderr").write_text(result.stderr)
        record["commands"].append(
            {
                "argv": command,
                "cwd": str(cwd),
                "exit": result.returncode,
                "wall_seconds": time.monotonic() - start,
            }
        )
        receipt.write_text(json.dumps(record, indent=2))
        result.check_returncode()
        return result.stdout

    record["commit"] = execute(
        ["git", "rev-parse", "HEAD"], args.output, "commit"
    ).strip()
    record["dirty"] = execute(
        ["git", "status", "--porcelain"], args.output, "status"
    ).strip()
    binary, record["consumer"] = build_consumer(args, execute)
    worker = [
        "nice",
        "-n",
        "10",
        sys.executable,
        ROOT / "test/experiments/weather/cloud_air_depth_reference.py",
        "--output",
        args.output,
    ]
    execute([*worker, "--task", "prepare"], args.output, "prepare")
    for state, defines in (
        ("advected", []),
        ("stationary", ["-DSOURCE_WIND_EAST_M=0", "-DSOURCE_WIND_SOUTH_M=0"]),
        ("clear", ["-USOURCE_CLOUD_COVER", "-DSOURCE_CLOUD_COVER=0"]),
    ):
        for profile, algorithm in PROFILES.items():
            name = f"{profile}-{state}"
            shader = args.output / f"{name}.spv"
            compile_kernel(
                args.output,
                shader,
                execute,
                "-DSOURCE_EYE_CLOUD_COVER=0",
                *algorithm,
                *defines,
            )
            for subset in ("", "sparse", "raster"):
                folder = args.output / subset
                record["profiles"][f"{subset or 'dense'}/{name}"] = run_capture(
                    args, binary, shader, folder, folder / f"{name}.bin", execute
                )
            if profile == "growth-128":
                folder = args.output / "empty"
                record["profiles"][f"empty/{state}"] = run_capture(
                    args, binary, shader, folder, folder / f"{name}.bin", execute
                )
    execute([*worker, "--task", "reference"], args.output, "reference")
    execute([*worker, "--task", "analyze"], args.output, "analyze")
    execute([*worker, "--task", "reconstruction"], args.output, "reconstruction")
    record["analysis"] = json.loads((args.output / "analysis.json").read_text())
    record["hashes"] = {
        str(path.relative_to(args.output)): digest(path)
        for path in args.output.rglob("*")
        if path.suffix in (".bin", ".spv", ".npy")
    }
    names = [
        "cloud_air_depth.py",
        "cloud_air_depth_cases.py",
        "cloud_air_depth_native.py",
        "cloud_air_depth_reference.py",
        "cloud_air_depth_source.glsl",
        "cloud_air_source.comp",
        "cloud_air_source.glsl",
        "cloud_air_source_reference.py",
        "cloud_air_reference.py",
        "cloud_field.py",
        "cloud_radiance.py",
    ]
    record["source_hashes"] = {
        name: digest(ROOT / "test/experiments/weather" / name) for name in names
    }
    receipt.write_text(json.dumps(record, indent=2))
    print("Full-depth native comparison passed; receipt:", receipt)


if __name__ == "__main__":
    main()
