"""Compare direct/hierarchical box queries on the same finished field, not native GPU performance."""

import argparse
import hashlib
import json
import platform
import tempfile
import time
from pathlib import Path

import numpy as np
from box_field_query import BoxField
from PIL import Image
from triangle_image_reference import box_grid_source, intersect_source, prepare_source


def load(path):
    if path.suffix == ".npz":
        with np.load(path, allow_pickle=False) as archive:
            chunks, tiers, palette, frame = (
                archive[k] for k in ("chunks", "tiers", "heights", "frame")
            )
        if not len(chunks) or chunks.ndim != 2 or chunks.shape[1] != 2:
            raise ValueError("nonempty finished urban patch product required")
        side = tiers.shape[1]
        low, high = chunks.min(0), chunks.max(0) + 1
        shape = (high - low) * side
        if np.prod(shape) > 1048576:
            raise ValueError("field exceeds the diagnostic cell budget")
        heights = np.zeros(tuple(shape[::-1]), np.float32)
        for (x, y), tile in zip(chunks - low, tiers, strict=True):
            heights[y * side : (y + 1) * side, x * side : (x + 1) * side] = palette[
                tile
            ]
        return heights, float(frame[2] * frame[3]), float(frame[4])
    text = path.read_text().split()
    columns, rows = map(int, text[:2])
    pitch, fill = map(float, text[2:4])
    return (
        np.array(list(map(float, text[4:])), np.float32).reshape(rows, columns),
        pitch,
        fill,
    )


def rays(field, width, height, altitude, supersample):
    side = max(field.rows, field.columns) * field.pitch
    eye = np.array([field.columns * field.pitch / 2, -side * 0.2, altitude])
    target = np.array(
        [field.columns * field.pitch / 2, field.rows * field.pitch / 2, 15.0]
    )
    forward = target - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, [0, 0, 1.0])
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    w, h = width * supersample, height * supersample
    x, y = np.meshgrid(
        (np.arange(w) + 0.5 - w / 2) / w, (h / 2 - np.arange(h) - 0.5) / w
    )
    direction = forward + x.reshape(-1, 1) * right + y.reshape(-1, 1) * up
    direction /= np.linalg.norm(direction, axis=1)[:, None]
    return np.broadcast_to(eye, direction.shape), direction


def image(depth, normal, origins, directions, width, height, supersample):
    sun = np.array([0.4, -0.6, 1.0])
    sun /= np.linalg.norm(sun)
    colour = (
        np.array([0.7, 0.65, 0.55]) * (0.2 + 0.8 * np.maximum(0, normal @ sun))[:, None]
    )
    floor = np.divide(
        -origins[:, 2],
        directions[:, 2],
        out=np.full(len(depth), np.inf),
        where=directions[:, 2] < 0,
    )
    floor[floor <= 0.1] = np.inf
    colour[np.isfinite(floor) & (floor < depth)] = [0.24, 0.28, 0.22]
    colour[~np.isfinite(floor) & ~np.isfinite(depth)] = [0.3, 0.45, 0.65]
    pixels = colour.reshape(height, supersample, width, supersample, 3).mean((1, 3))
    return (np.clip(pixels, 0, 1) * 255).astype(np.uint8)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=160)
    parser.add_argument("--height", type=int, default=90)
    parser.add_argument("--altitude", type=float, default=100)
    parser.add_argument("--supersample", type=int, default=1)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--oracle-rays", type=int, default=0)
    args = parser.parse_args()
    output = args.output.resolve()
    if Path(tempfile.gettempdir()).resolve() not in output.parents:
        parser.error("output must be inside the system temporary directory")
    if min(args.width, args.height, args.supersample, args.rounds) < 1:
        parser.error("positive output dimensions, sampling and rounds required")
    if args.width * args.height * args.supersample**2 > 4194304:
        parser.error("ray packet exceeds the diagnostic budget")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty")
    heights, pitch, fill = load(args.input)
    begin = time.perf_counter()
    field = BoxField(heights, pitch, fill)
    results = {
        "build_ms": (time.perf_counter() - begin) * 1000,
        "retained_bytes": field.retained_bytes,
        "shape": list(heights.shape),
        "occupied": int(np.count_nonzero(heights)),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "arguments": vars(args) | {"input": str(args.input), "output": str(output)},
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
    }
    origins, directions = rays(
        field, args.width, args.height, args.altitude, args.supersample
    )
    results["rays"] = len(origins)
    results["pitch_m"], results["fill_fraction"] = pitch, fill
    results["host"] = platform.platform()
    results["source_sha256"] = {
        name: hashlib.sha256((Path(__file__).parent / name).read_bytes()).hexdigest()
        for name in (
            "bench_box_field.py",
            "box_field_query.py",
            "column_field_query.py",
            "triangle_image_reference.py",
        )
    }
    snapshots, panels = {}, []
    for method in ("direct", "hierarchy"):
        samples = []
        for _ in range(args.rounds):
            begin = time.perf_counter()
            depth, normal, work = getattr(field, "trace_" + method)(origins, directions)
            samples.append((time.perf_counter() - begin) * 1000)
        snapshots[method] = (depth, normal)
        results[method] = {
            "ms": samples,
            "work": work,
            "finite": int(np.isfinite(depth).sum()),
        }
        panels.append(
            image(
                depth,
                normal,
                origins,
                directions,
                args.width,
                args.height,
                args.supersample,
            )
        )
    a, b = snapshots["direct"], snapshots["hierarchy"]
    np.testing.assert_array_equal(np.isfinite(a[0]), np.isfinite(b[0]))
    good = np.isfinite(a[0])
    np.testing.assert_allclose(a[0][good], b[0][good], atol=1e-8, rtol=1e-10)
    np.testing.assert_allclose(a[1][good], b[1][good], atol=1e-10, rtol=0)
    if args.oracle_rays:
        source = box_grid_source(heights, pitch, fill)
        prepared = prepare_source(source)
        ids = np.random.default_rng(7).choice(
            len(origins), min(args.oracle_rays, len(origins)), replace=False
        )
        expected, _ = intersect_source(
            source, origins[ids], directions[ids], prepared=prepared
        )
        np.testing.assert_array_equal(np.isfinite(expected), np.isfinite(a[0][ids]))
        good = np.isfinite(expected)
        np.testing.assert_allclose(
            expected[good], a[0][ids][good], atol=1e-8, rtol=1e-10
        )
        results["triangle_oracle_rays"] = len(ids)
    Image.fromarray(np.concatenate(panels, axis=1)).save(output / "comparison.png")
    (output / "result.json").write_text(json.dumps(results, indent=2))
    print(json.dumps({k: results[k] for k in ("shape", "occupied", "retained_bytes")}))


if __name__ == "__main__":
    main()
