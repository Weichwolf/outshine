"""Render bounded cloud footprint cohorts or 1280x720 image tiles through Native."""

import argparse
import hashlib
import json
import os
import shlex
import subprocess
from pathlib import Path

import numpy as np
from cloud_radiance import save_picture

ROOT = Path(__file__).resolve().parents[3]
TILE_WIDTH, TILE_HEIGHT = 160, 90


def invoke(command, name, output, runs):
    result = subprocess.run(
        command, cwd=ROOT, capture_output=True, text=True, timeout=40, check=False
    )
    (output / f"{name}.log").write_text(
        result.stdout + result.stderr + f"\nExit: {result.returncode}\n"
    )
    runs.append(
        {
            "name": name,
            "command": command,
            "exit_code": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
        }
    )
    (output / "runs.json").write_text(json.dumps(runs, indent=2) + "\n")
    if result.returncode or (name.startswith("capture") and result.stderr):
        raise RuntimeError(f"{name} failed; see {output / (name + '.log')}")


def capture(args, name, defines, width, height, runs):
    shader, result = args.output / f"{name}.spv", args.output / f"{name}.f32"
    compiler = shlex.split(os.environ.get("GLSLANG", "glslangValidator"))
    invoke(
        [
            "nice",
            "-n",
            "10",
            *compiler,
            "-V",
            "--target-env",
            "vulkan1.0",
            "-Isrc",
            *defines,
            "test/experiments/weather/cloud_picture.comp",
            "-o",
            str(shader),
        ],
        "compile-" + name,
        args.output,
        runs,
    )
    invoke(
        [
            str(args.probe),
            str(shader),
            str(result),
            str(width),
            str(height),
            str(args.cover),
            "4",
        ],
        "capture-" + name,
        args.output,
        runs,
    )
    values = np.fromfile(result, dtype=np.float32).reshape(height, width, 4)
    if (
        not np.isfinite(values).all()
        or (values < 0).any()
        or (values[..., 3] > 1).any()
    ):
        raise RuntimeError(f"{name}: invalid native RGB/T output")
    runs[-1]["output_sha256"] = hashlib.sha256(result.read_bytes()).hexdigest()
    return values, result


def image(args, side, defines, runs):
    values = np.zeros((720, 1280, 4), dtype=np.float64)
    for y in range(0, 720, TILE_HEIGHT):
        for x in range(0, 1280, TILE_WIDTH):
            name = f"tile-{x}-{y}"
            tile_defines = [
                *defines,
                "-DCLOUD_FOOTPRINT_IMAGE=1",
                f"-DCLOUD_TILE_X={x}",
                f"-DCLOUD_TILE_Y={y}",
            ]
            width, height = TILE_WIDTH, TILE_HEIGHT
            if args.independent_rays:
                tile_defines += ["-DCLOUD_FOOTPRINT_SAMPLES=1"]
                width, height = width * side, height * side
            tile, raw = capture(args, name, tile_defines, width, height, runs)
            if args.independent_rays:
                tile = tile.reshape(TILE_HEIGHT, side, TILE_WIDTH, side, 4)
                tile = tile.astype(np.float64).mean(axis=(1, 3))
            values[y : y + TILE_HEIGHT, x : x + TILE_WIDTH] = tile
            raw.unlink()
            runs[-1]["transient_ray_output_removed"] = True
            print(f"completed {name}", flush=True)
    values.astype(np.float32).tofile(args.output / "image.f32")
    save_picture(values, args.output / "image.png")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--probe", type=Path, default=ROOT / "build/cloud-native-probe")
    parser.add_argument("--mode", choices=["cohort", "image"], default="cohort")
    parser.add_argument(
        "--sides", type=int, nargs="+", default=[1, 2, 4, 8, 16, 32, 64]
    )
    parser.add_argument("--pattern", choices=["grid", "hammersley"], default="grid")
    parser.add_argument("--view-samples", type=int, default=1024)
    parser.add_argument("--light-samples", type=int, default=64)
    parser.add_argument("--warp", type=int, choices=[1, 2], default=1)
    parser.add_argument("--cover", type=float, default=0.65)
    parser.add_argument("--independent-rays", action="store_true")
    args = parser.parse_args()
    if (
        any(side < 1 or side > 64 or side & (side - 1) for side in args.sides)
        or not 1 <= args.view_samples <= 4096
        or not 1 <= args.light_samples <= 256
        or not np.isfinite(args.cover)
        or not 0 <= args.cover <= 1
        or (args.mode == "image" and (len(args.sides) != 1 or args.sides[0] > 8))
    ):
        parser.error(
            "require bounded samples, power-of-two sides and one side <=8 for an image"
        )
    args.output, args.probe = args.output.resolve(), args.probe.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "receipt.json").unlink(missing_ok=True)
    sources = [
        ROOT / name
        for name in (
            "test/experiments/weather/cloud_picture.comp",
            "test/experiments/weather/cloud_footprint_native.py",
            "src/render/shaders/cloudLayer.glsl",
            "src/render/shaders/cloudRadiance.glsl",
        )
    ]
    sources += [args.probe]
    before = {
        str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources
    }
    runs = []
    for side in args.sides:
        defines = [
            f"-DCLOUD_PIXEL_SIDE={side}",
            f"-DCLOUD_VIEW_SAMPLES={args.view_samples}",
            f"-DCLOUD_LIGHT_SAMPLES={args.light_samples}",
            f"-DCLOUD_VIEW_WARP={args.warp}",
        ]
        if args.pattern == "hammersley":
            defines += ["-DCLOUD_PIXEL_HAMMERSLEY=1"]
        if args.mode == "image":
            image(args, side, defines, runs)
        else:
            capture(
                args,
                f"{args.pattern}-{side}",
                [*defines, "-DCLOUD_FOOTPRINT_COHORT=1", "-DCLOUD_FOOTPRINT_SAMPLES=1"],
                8 * side,
                8 * side,
                runs,
            )
            print(f"completed {args.pattern}-{side}", flush=True)
    if any(
        hashlib.sha256(path.read_bytes()).hexdigest() != before[str(path)]
        for path in sources
    ):
        raise RuntimeError("render inputs changed during capture")
    resolve = (
        "unresolved independent GPU rays"
        if args.mode == "cohort"
        else (
            "offline Float64 average"
            if args.independent_rays
            else "native per-pixel average"
        )
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=ROOT, text=True
            ).strip()
        ),
        "parameters": {
            k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()
        },
        "resolve": resolve,
        "source_and_binary_sha256": before,
        "runs": runs,
        "GPU_ms": "unmeasured",
        "scope": "Declared static diagnostic; no Place/temporal/budget acceptance.",
    }
    (args.output / "receipt.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
