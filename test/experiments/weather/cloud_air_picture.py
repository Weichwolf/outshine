"""Render a native prefix diagnostic with a spherical placeholder, not a Place."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from cloud_air_reference import RAY, SAMPLE
from cloud_comparison import view_rays
from cloud_field import sphere_roots
from cloud_radiance import save_picture


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tile_rays(width, height, x, y, tile_width, tile_height, medium):
    px, py = np.meshgrid(
        np.arange(x, x + tile_width) + 0.5, np.arange(y, y + tile_height) + 0.5
    )
    direction = view_rays(
        width, height, 240.0, -0.5, 33.97, pixel_centers=np.stack([px, py], axis=-1)
    ).reshape(-1, 3)
    rays = np.zeros(len(direction), dtype=RAY)
    rays["ray"][:, 0] = float(medium[0]) + 1.772
    rays["ray"][:, 1] = direction[:, 1]
    rays["ray"][:, 2] = 0.8
    rays["azimuth"] = np.arctan2(direction[:, 2], direction[:, 0])
    horizontal = np.sqrt(np.maximum(0.0, 1.0 - rays["ray"][:, 1].astype(float) ** 2))
    physical = np.column_stack(
        [
            horizontal * np.cos(rays["azimuth"].astype(float)),
            rays["ray"][:, 1],
            horizontal * np.sin(rays["azimuth"].astype(float)),
        ]
    )
    rays["ray"][:, 3] = physical @ np.array([-0.6, 0.8, 0.0])
    eye_m = float(np.float32(np.float32(rays["ray"][0, 0] - medium[0]) * 1000.0))
    origin = np.array([0.0, eye_m, 0.0])
    radius_m = float(medium[0]) * 1000.0
    _, top_far, _ = sphere_roots(
        origin, physical, float(medium[1] - medium[0]) * 1000.0, radius_m
    )
    near, _, ground_hit = sphere_roots(origin, physical, 0.0, radius_m)
    ground = ground_hit & (near >= 0.0)
    distances = (np.where(ground, near, top_far) * 0.001).astype("<f4")
    rays["ray"][:, 4] = distances
    rays["begin"] = np.arange(len(rays), dtype="<u4")
    rays["end"] = rays["begin"] + 1
    return rays, distances, ground


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--shader", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    args = parser.parse_args()
    if not 1 <= args.width <= 3840 or not 1 <= args.height <= 2160:
        raise ValueError("diagnostic dimensions outside the declared input budget")
    for name in ("shader", "probe", "fixtures", "output"):
        setattr(args, name, getattr(args, name).resolve())
    args.output.mkdir(parents=True, exist_ok=True)
    receipt = args.output / "receipt.json"
    receipt.unlink(missing_ok=True)
    medium = np.fromfile(args.fixtures / "medium.bin", dtype="<f4")
    transfer = np.empty((args.height, args.width, 6), dtype="<f4")
    rgb = np.empty((args.height, args.width, 3), dtype="<f4")
    rows = []
    work_sum = np.zeros(4, dtype=np.uint64)
    work_max = np.zeros(4, dtype=np.uint32)
    for y in range(0, args.height, 90):
        for x in range(0, args.width, 160):
            width, height = min(160, args.width - x), min(90, args.height - y)
            rays, distances, ground = tile_rays(
                args.width, args.height, x, y, width, height, medium
            )
            rays.tofile(args.output / "rays.bin")
            distances.tofile(args.output / "queries.bin")
            command = [
                str(args.probe),
                str(args.shader),
                str(args.output / "rays.bin"),
                str(args.output / "queries.bin"),
                str(args.fixtures / "veil.bin"),
                str(args.fixtures / "multi.bin"),
                str(args.fixtures / "medium.bin"),
                str(args.output / "native.bin"),
            ]
            result = subprocess.run(
                command, capture_output=True, text=True, timeout=40, check=False
            )
            rows.append(
                {
                    "tile": [x, y, width, height],
                    "command": command,
                    "exit_code": result.returncode,
                    "stdout": result.stdout,
                    "stderr": result.stderr,
                    "rays_sha256": digest(args.output / "rays.bin"),
                    "queries_sha256": digest(args.output / "queries.bin"),
                }
            )
            (args.output / "runs.json").write_text(json.dumps(rows, indent=2))
            if result.returncode or result.stderr:
                raise RuntimeError(f"native capture failed: {rows[-1]}")
            samples = np.fromfile(args.output / "native.bin", dtype=SAMPLE)
            values = np.concatenate([samples["radiance"], samples["through"]], axis=1)
            assert len(samples) == width * height and np.isfinite(values).all()
            assert (values >= 0).all() and (samples["through"] <= 1).all()
            assert (samples["counts"] & 255).max() == 0
            work = np.column_stack(
                [
                    (samples["counts"] >> 16) & 255,
                    (samples["counts"] >> 8) & 255,
                    samples["sources"],
                    samples["counts"] >> 24,
                ]
            )
            work_sum += work.sum(axis=0, dtype=np.uint64)
            work_max = np.maximum(work_max, work.max(axis=0))
            background = np.where(ground[:, None], [0.04, 0.06, 0.025], [0.0, 0.0, 0.0])
            transfer[y : y + height, x : x + width] = values.reshape(height, width, 6)
            composed = samples["radiance"] + samples["through"] * background
            rgb[y : y + height, x : x + width] = composed.reshape(height, width, 3)
        print(f"captured rows {y + height}/{args.height}", flush=True)
    transfer.tofile(args.output / "transfer.f32")
    save_picture(rgb, args.output / "image.png")
    for name in ("rays.bin", "queries.bin", "native.bin"):
        (args.output / name).unlink()
    receipt.write_text(
        json.dumps(
            {
                "resolution": [args.width, args.height],
                "commit": subprocess.check_output(
                    ["git", "rev-parse", "HEAD"], text=True
                ).strip(),
                "working_tree_dirty": bool(
                    subprocess.check_output(["git", "status", "--porcelain"])
                ),
                "shader_sha256": digest(args.shader),
                "consumer_sha256": digest(args.probe),
                "image_sha256": digest(args.output / "image.png"),
                "transfer_sha256": digest(args.output / "transfer.f32"),
                "captures": len(rows),
                "packed_work": {
                    name: {"sum": int(total), "max": int(peak)}
                    for name, total, peak in zip(
                        ("view_density", "cloud_source", "clear_source", "union_cells"),
                        work_sum,
                        work_max,
                        strict=True,
                    )
                },
                "scope": "Native full physical ray extent; supplied source-LUT model, fixed cloud lighting, spherical placeholder. No terrain/asset/Place or numerical convergence/frame-budget claim.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
