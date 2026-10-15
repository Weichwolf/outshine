"""Independent pixel-area RGB/T reference for the declared cloud horizon (#264)."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
from cloud_comparison import view_rays
from cloud_field import Layer
from cloud_radiance import background, radiance

WIDTH, HEIGHT = 1280, 720
ROWS = (232, 256, 280, 304, 328, 352, 368, 378)


def cohort():
    x, y = np.meshgrid(np.arange(80, WIDTH, 160), ROWS)
    return np.stack([x.ravel(), y.ravel()], axis=-1)


def offsets(side, pattern):
    count = side * side
    if pattern == "hammersley":
        index = np.arange(count, dtype=np.uint32)
        value, reverse = index.copy(), np.zeros(count, dtype=np.uint32)
        for _ in range(32):
            reverse = (reverse << 1) | (value & 1)
            value >>= 1
        return np.stack(
            [(index + 0.5) / count, reverse.astype(np.float64) / 2**32 + 0.5 / count],
            axis=-1,
        )
    x, y = np.meshgrid((np.arange(side) + 0.5) / side, (np.arange(side) + 0.5) / side)
    return np.stack([x.ravel(), y.ravel()], axis=-1)


def evaluate(side, pattern, view_samples, light_samples, cover, warp=1):
    samples = cohort()[:, None, :] + offsets(side, pattern)
    direction = view_rays(WIDTH, HEIGHT, 240.0, -0.5, 33.97, pixel_centers=samples)
    layer, origin = Layer(cover=cover), np.array([0.0, 1772.0, 0.0])
    sky, maximum = background(origin, direction, layer)
    cloud = radiance(
        origin,
        direction,
        layer,
        maximum,
        view_samples=view_samples,
        light_samples=light_samples,
        warp=warp,
    )
    composed = np.concatenate(
        [cloud[..., :3] + cloud[..., 3:4] * sky, cloud[..., 3:4]], axis=-1
    )
    ground = np.isfinite(maximum)
    return composed.mean(axis=-2), ground


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sides", type=int, nargs="+", default=[1, 2, 4, 8])
    parser.add_argument("--pattern", choices=["grid", "hammersley"], default="grid")
    parser.add_argument("--view-samples", type=int, default=256)
    parser.add_argument("--light-samples", type=int, default=16)
    parser.add_argument("--cover", type=float, default=0.65)
    parser.add_argument("--warp", type=int, choices=[1, 2], default=1)
    native_inputs = parser.add_mutually_exclusive_group()
    native_inputs.add_argument("--native-dir", type=Path)
    native_inputs.add_argument("--native-samples-dir", type=Path)
    args = parser.parse_args()
    if (
        any(side < 1 or side > 64 or side & (side - 1) for side in args.sides)
        or not 1 <= args.view_samples <= 4096
        or not 1 <= args.light_samples <= 256
        or not np.isfinite(args.cover)
        or not 0 <= args.cover <= 1
    ):
        parser.error(
            "require power-of-two sides in [1,64], bounded sample counts and cover in [0,1]"
        )
    args.output.mkdir(parents=True, exist_ok=True)
    report = {
        "pixels": cohort().tolist(),
        "resolution": [WIDTH, HEIGHT],
        "camera": {
            "height_m": 1772.0,
            "bearing_deg": 240.0,
            "pitch_deg": -0.5,
            "fov_deg": 33.97,
        },
        "cover": args.cover,
        "pattern": args.pattern,
        "view_samples": args.view_samples,
        "light_samples": args.light_samples,
        "warp": args.warp,
        "pixel_kernel": "uniform box, linear composed RGB and cloud transmittance",
        "scope": "Declared Float64 RGB/T cohort; finite quadrature, no general convergence/temporal/budget acceptance.",
        "runs": [],
    }
    for side in args.sides:
        before = time.perf_counter()
        values, ground = evaluate(
            side,
            args.pattern,
            args.view_samples,
            args.light_samples,
            args.cover,
            args.warp,
        )
        assert (
            np.isfinite(values).all()
            and (values >= 0).all()
            and (values[:, 3] <= 1).all()
        )
        np.savez(
            args.output / f"{args.pattern}-{side}.npz", values=values, ground=ground
        )
        record = {
            "side": side,
            "rays": 64 * side * side,
            "wall_s": time.perf_counter() - before,
            "silhouette_pixels": int(
                (ground.any(axis=-1) & ~ground.all(axis=-1)).sum()
            ),
        }
        native_dir = args.native_samples_dir or args.native_dir
        if native_dir:
            native = np.fromfile(
                native_dir / f"{args.pattern}-{side}.f32", dtype=np.float32
            )
            if args.native_samples_dir:
                native = native.reshape(8, side, 8, side, 4).astype(np.float64)
                native = native.mean(axis=(1, 3)).reshape(64, 4)
                record["native_resolve"] = (
                    "offline Float64 average of independent GPU rays"
                )
            else:
                native = native.reshape(64, 4)
            assert (
                np.isfinite(native).all()
                and (native >= 0).all()
                and (native[:, 3] <= 1).all()
            )
            error = np.abs(values - native)
            record.update(
                native_max_abs=float(error.max()),
                native_p99_abs=float(np.quantile(error, 0.99)),
            )
            assert error.max() < 0.0001
        report["runs"].append(record)
        (args.output / "reference.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(record), flush=True)


if __name__ == "__main__":
    main()
