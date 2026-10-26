"""Measure angular sharing without filtering actual eye-cloud transfer (#353)."""

import argparse
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

import numpy as np
from cloud_sun_reuse_benchmark import load_snapshot
from cloud_sun_reuse_native import ROOT, build_consumer, digest, gpu_times


def compile_kernel(output, target, execute, *defines, reconstruct=False):
    name = "cloud_air_transfer_reuse.comp" if reconstruct else "cloud_air_source.comp"
    execute(
        [
            "nice",
            "-n",
            "10",
            *shlex.split(os.environ.get("GLSLANG", "glslangValidator")),
            "-V",
            "-DAIR_PREFIX_INTERVALS=256",
            "-DSOURCE_GRID_LOG=1",
            "-DSOURCE_CLOUD_MODE=0",
            "-DSOURCE_CLOUD_COVER=0.65",
            *defines,
            "-Isrc",
            "-Isrc/render/shaders",
            "-Isrc/generators/weather/shaders",
            "-Itest/experiments/weather",
            "-Itest/experiments/render",
            f"-I{output}",
            ROOT / "test/experiments/weather" / name,
            "-o",
            target,
        ],
        output,
        "compile-" + target.stem,
    )


def run_capture(args, binary, shader, fixtures, target, execute, source_map=None):
    before = load_snapshot()
    command = [
        binary,
        shader,
        fixtures / "rays.bin",
        fixtures / "queries.bin",
        *(args.output / f"{n}.bin" for n in ("veil", "multi", "medium")),
        target,
        args.repeats,
    ]
    if source_map is not None:
        command += ["--source-map", source_map]
    command += ["--gpu-timing"]
    log = execute(command, target.parent, target.stem)
    values = gpu_times(log)
    return {
        "dispatch_ms": values,
        "warm_median_ms": float(np.median(values[1:])),
        "load_before": before,
        "load_after": load_snapshot(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--api-root", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--archive-receipt", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=3)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT) or args.repeats < 1:
        parser.error("use an explicit system-temp output and at least one warm repeat")
    args.output.mkdir(parents=True, exist_ok=True)
    record = {"commands": [], "profiles": {}}
    receipt = args.output / "receipt.json"

    def execute(command, folder, name, cwd=ROOT):
        command = list(map(str, command))
        result = subprocess.run(
            command, check=False, cwd=cwd, capture_output=True, text=True
        )
        (folder / f"{name}.stdout").write_text(result.stdout)
        (folder / f"{name}.stderr").write_text(result.stderr)
        record["commands"].append(
            {"argv": command, "cwd": str(cwd), "exit": result.returncode}
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
        ROOT / "test/experiments/weather/cloud_air_transfer_cases.py",
        "--output",
        args.output,
    ]
    execute([*worker, "--task", "prepare"], args.output, "prepare")
    for name, cover in (("joint", "0.65"), ("air", "0")):
        shader = args.output / f"{name}.spv"
        compile_kernel(
            args.output, shader, execute, f"-DSOURCE_EYE_CLOUD_COVER={cover}"
        )
        record["profiles"][name] = run_capture(
            args, binary, shader, args.output, args.output / f"{name}.bin", execute
        )
    for step in (16, 8, 4):
        folder = args.output / f"step-{step}"
        producer = run_capture(
            args,
            binary,
            args.output / "air.spv",
            folder,
            folder / "producer.bin",
            execute,
        )
        execute([*worker, "--task", "field", "--step", step], folder, "field")
        shader = folder / "reconstruct.spv"
        compile_kernel(folder, shader, execute, reconstruct=True)
        reconstruction = run_capture(
            args,
            binary,
            shader,
            args.output,
            folder / "reconstructed.bin",
            execute,
            folder / "visibility.bin",
        )
        record["profiles"][str(step)] = {
            "producer": producer,
            "reconstruction": reconstruction,
        }
    execute([*worker, "--task", "analyze"], args.output, "analyze")
    record["analysis"] = json.loads((args.output / "analysis.json").read_text())
    record["hashes"] = {
        str(p.relative_to(args.output)): digest(p)
        for p in args.output.rglob("*")
        if p.suffix in (".bin", ".spv", ".glsl")
    }
    sources = [
        ROOT / "test/experiments/render/air_prefix_gpu.cpp",
        *sorted((ROOT / "test/experiments/weather").glob("*.py")),
        *sorted((ROOT / "test/experiments/weather").glob("*.comp")),
        *sorted((ROOT / "test/experiments/weather").glob("*.glsl")),
        *sorted((ROOT / "src/render/shaders").glob("*.glsl")),
        *sorted((ROOT / "src/generators/weather/shaders").glob("*.glsl")),
        *sorted((ROOT / "src/world/sky").glob("*.h")),
    ]
    record["source_hashes"] = {str(p.relative_to(ROOT)): digest(p) for p in sources}
    record["runtime_profile_accepted"] = False
    receipt.write_text(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
