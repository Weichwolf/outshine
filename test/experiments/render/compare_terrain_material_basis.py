"""Compare fixed-footprint material blocks against current bound native recipes."""

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
import time
from functools import partial
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from probe_terrain_material_substrate import optical_row
from relief_shading_reference import specular
from terrain_material_basis import bake, sample
from terrain_material_litter import compose


def timed(function):
    start = time.perf_counter_ns()
    result = function()
    return result, (time.perf_counter_ns() - start) / 1e6


def normals(raw, dx, dy):
    gradient = raw[:, 4:] @ np.linalg.inv(np.stack((dx, dy))).T
    n = np.column_stack((-gradient[:, 0], np.ones(len(raw)), -gradient[:, 1]))
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def radiance(raw, dx, dy):
    """Diagnostic GGX/diffuse, without native sky, class/rock blend or shadows."""
    n = normals(raw, dx, dy)
    light, view = np.array([0.5, 0.8, -0.2]), np.array([0, 1, 0.6])
    light /= np.linalg.norm(light)
    view /= np.linalg.norm(view)
    diffuse = (
        np.clip(raw[:, :3], 0, 1)
        * (0.12 + 0.96 * np.maximum(n @ light, 0))[:, None]
        / np.pi
    )
    return diffuse + specular(n, np.clip(raw[:, 3], 0.04, 1), light, view)[:, None]


def errors(expected, actual, dx, dy):
    normal_dot = np.sum(normals(expected, dx, dy) * normals(actual, dx, dy), axis=-1)
    groups = {
        "albedo_linear": np.abs(expected[:, :3] - actual[:, :3]),
        "roughness": np.abs(expected[:, 3] - actual[:, 3]),
        "normal_deg": np.degrees(np.arccos(np.clip(normal_dot, -1, 1))),
        "diagnostic_radiance": np.abs(
            radiance(expected, dx, dy) - radiance(actual, dx, dy)
        ),
    }
    return {
        name: {
            "rms": float(np.sqrt(np.mean(delta**2))),
            "p99": float(np.quantile(delta, 0.99)),
            "max": float(delta.max()),
        }
        for name, delta in groups.items()
    }


def load_capture(path):
    receipt = json.loads((path / "receipt.json").read_text())
    raw = (path / "palette.f32").read_bytes()
    if (
        len(raw) != receipt["palette"]["bytes"]
        or hashlib.sha256(raw).hexdigest() != receipt["palette"]["sha256"]
        or not receipt["gpu_bytes_equal"]
    ):
        raise ValueError("actual CPU/GPU-equal native palette capture required")
    words = np.frombuffer(raw, dtype="<u4")
    rows, layers = int(words[0]), int(words[3])
    if (
        not 0 < rows <= 256
        or layers != 4 * (rows + 2) + 9 * (rows + 1)
        or len(words) != layers + 8 * (rows + 1)
    ):
        raise ValueError("known current native palette layout required")
    return (
        np.frombuffer(raw, dtype="<f4"),
        {"rows": rows, "layer_offset_floats": layers},
        receipt,
    )


def image(raw, dx, dy):
    linear = np.clip(radiance(raw, dx, dy), 0, 1)
    rgb = np.where(
        linear <= 0.0031308, 12.92 * linear, 1.055 * linear ** (1 / 2.4) - 0.055
    )
    return Image.fromarray(np.uint8(rgb.reshape(64, 64, 3) * 255)).resize((256, 256))


def compare(palette, layout, receipt, output):
    records, worst = [], []
    origin = np.asarray(receipt["fine"]["origin_m"]) + [0.137, 0.271]
    axis = (np.arange(64) + 0.5) / 64
    x, y = np.meshgrid(axis, axis)
    fraction = np.column_stack((x.ravel(), y.ravel()))
    angle = 0.731
    rotations = {
        "axis": np.eye(2),
        "rotated": np.array(
            [[np.cos(angle), np.sin(angle)], [-np.sin(angle), np.cos(angle)]]
        ),
        "anisotropic16": np.array(
            [[np.cos(angle), np.sin(angle)], [-np.sin(angle), np.cos(angle)]]
        )
        * [[1], [1 / 16]],
    }
    for row in range(layout["rows"]):
        evaluate = partial(compose, **optical_row(palette, layout, row))
        for footprint in (0.0001, 0.001, 0.01, 0.1, 1.0, 10.0):
            extent = 16 * footprint
            at = origin + fraction * extent
            expected = {}
            for mode, directions in rotations.items():
                derivatives = directions * footprint
                dx, dy = (np.broadcast_to(d, at.shape) for d in derivatives)
                expected[mode] = timed(partial(evaluate, at, dx, dy))
            for side in (17, 33, 65):
                for dtype in ("float32", "float16"):
                    basis, build_ms = timed(
                        partial(
                            bake, evaluate, origin, extent, footprint, side, dtype=dtype
                        )
                    )
                    for mode, directions in rotations.items():
                        dx, dy = (
                            np.broadcast_to(d * footprint, at.shape) for d in directions
                        )
                        actual, _ = timed(partial(sample, basis, at, dx, dy))
                        read_ms = [
                            timed(partial(sample, basis, at, dx, dy))[1]
                            for _ in range(3)
                        ]
                        reference, direct_ms = expected[mode]
                        metrics = errors(reference, actual, dx[0], dy[0])
                        record = {
                            "row": row,
                            "footprint_m": footprint,
                            "side": side,
                            "dtype": dtype,
                            "orientation": mode,
                            "retained_bytes": basis.retained_bytes,
                            "bake_ms": build_ms,
                            "direct_ms": direct_ms,
                            "read_median_ms": float(np.median(read_ms)),
                            "errors": metrics,
                        }
                        records.append(record)
                        score = metrics["diagnostic_radiance"]["p99"]
                        same_patch = next(
                            (
                                i
                                for i, entry in enumerate(worst)
                                if (entry[1]["row"], entry[1]["footprint_m"])
                                == (row, footprint)
                            ),
                            None,
                        )
                        if same_patch is not None:
                            if score <= worst[same_patch][0]:
                                continue
                            worst.pop(same_patch)
                        if len(worst) < 4 or score > worst[-1][0]:
                            worst.append(
                                (
                                    score,
                                    record,
                                    reference.copy(),
                                    actual.copy(),
                                    dx[0].copy(),
                                    dy[0].copy(),
                                )
                            )
                            worst.sort(key=lambda entry: -entry[0])
                            worst = worst[:4]
    canvas = Image.new("RGB", (768, len(worst) * 280), "white")
    draw = ImageDraw.Draw(canvas)
    for i, (_, record, expected, actual, dx, dy) in enumerate(worst):
        draw.text(
            (4, i * 280 + 4),
            f"row {record['row']} / {record['footprint_m']}m / {record['side']} / {record['dtype']} / {record['orientation']} | analytic / grid / error x16",
            fill="black",
        )
        canvas.paste(image(expected, dx, dy), (0, i * 280 + 24))
        canvas.paste(image(actual, dx, dy), (256, i * 280 + 24))
        delta = np.clip(
            np.abs(radiance(expected, dx, dy) - radiance(actual, dx, dy)) * 16, 0, 1
        )
        canvas.paste(
            Image.fromarray(np.uint8(delta.reshape(64, 64, 3) * 255)).resize(
                (256, 256)
            ),
            (512, i * 280 + 24),
        )
    canvas.save(output / "counterexamples.png")
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output = args.output.resolve()
    if not any(
        args.output.is_relative_to(path)
        for path in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private comparison output belongs in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    palette, layout, receipt = load_capture(args.capture)
    records = compare(palette, layout, receipt, args.output)
    summary = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "native_commit": receipt["native_commit"],
        "palette": receipt["palette"],
        "capture_receipt_sha256": hashlib.sha256(
            (args.capture / "receipt.json").read_bytes()
        ).hexdigest(),
        "command": [sys.executable, *sys.argv],
        "python": sys.version,
        "numpy": np.__version__,
        "source_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                Path(__file__).name,
                "terrain_material_basis.py",
                "terrain_material_grid.py",
                "terrain_material_litter.py",
                "terrain_material_substrate.py",
                "terrain_material_plane.py",
                "probe_terrain_material_substrate.py",
                "relief_shading_reference.py",
            )
        },
        "scope": "Controlled metric patches inside captured class-grid domain, all actual palette rows; current substrate/litter response only.",
        "limitations": [
            "Not native field coordinates or rendered terrain fragments.",
            "No class boundary/rock blending or whole-world residency/IO.",
            "No NDF/correlated roughness moments; GGX images are diagnostic only.",
            "Python CPU timings are not GPU or frame-time savings.",
            "Fixed footprint levels; anisotropic orientation mismatch is measured, not hidden.",
        ],
        "queries_per_case": 4096,
        "cases": records,
    }
    (args.output / "comparison.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(args.output / "comparison.json")


if __name__ == "__main__":
    main()
