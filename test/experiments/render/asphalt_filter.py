"""Select bounded dry-asphalt parameter filters against independent box pixels."""

import argparse
import hashlib
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from asphalt_field import filtered, grain, parameters, pixel_reference, point_field

METHODS = (
    "point",
    "isotropic",
    "jacobian",
    "four",
    "jacobian_minor",
    "four_minor",
    "oriented",
)


def srgb(linear):
    return np.where(
        linear <= 0.0031308, linear * 12.92, 1.055 * linear ** (1 / 2.4) - 0.055
    )


def cases():
    phase = np.linspace(-0.5, 0.5, 17)
    motion = np.column_stack([0.13 + phase, -0.17 + 0.37 * np.cos(phase * 3)])
    centres, _ = grain(np.array([[0, 0], [1, -3], [-2, 1], [7, -5], [-11, 13]]))
    rings = np.concatenate(
        [centres + offset for offset in ((0, 0), (0.12, 0), (0.24, 0), (0.12, 0.12))]
    )
    random = np.random.default_rng(781).uniform(-16, 16, (27, 2))
    points = np.concatenate([motion, rings, random])
    for width in (0.025, 0.125, 0.25, 0.5, 1, 2, 4):
        for shape, dx, dy in (
            ("square", [1, 0], [0, 1]),
            ("rotated", [0.8, 0.6], [-0.6, 0.8]),
            ("sheared", [1, 0.3], [-0.6, 1]),
            ("thin", [1, 0.3], [0, 1 / 64]),
        ):
            yield f"{shape}-{width}", points, width * np.array(dx), width * np.array(dy)
    yield "zero", points, np.zeros(2), np.zeros(2)


def converged_reference(points, dx, dy):
    previous = None
    for order in (16, 32, 64, 128):
        result = pixel_reference(points, dx, dy, order)
        if previous is not None:
            delta = float(np.max(np.abs(result - previous)))
            if delta <= 1e-6:
                return result, order, delta
        previous = result
    raise ValueError(f"box reference failed convergence: {delta}")


def compare(output):
    rows = []
    for name, points, dx, dy in cases():
        started = time.perf_counter()
        reference, order, convergence = converged_reference(points, dx, dy)
        reference_seconds = time.perf_counter() - started
        ref_colour, ref_roughness = parameters(reference)
        row = {
            "name": name,
            "points": points.tolist(),
            "dx": dx.tolist(),
            "dy": dy.tolist(),
            "reference_order": order,
            "reference_convergence": convergence,
            "reference_seconds": reference_seconds,
            "methods": [],
        }
        raw = {"reference": reference}
        for method in METHODS:
            started = time.perf_counter()
            actual = filtered(points, dx, dy, method)
            seconds = time.perf_counter() - started
            colour, roughness = parameters(actual)
            if (
                not np.all(np.isfinite(actual))
                or np.any(colour < 0)
                or np.any(colour > 1)
                or np.any(roughness < 0)
                or np.any(roughness > 1)
            ):
                raise ValueError("invalid filtered material parameters")
            colour_error = np.max(np.abs(srgb(colour) - srgb(ref_colour)), axis=1) * 255
            rough_error = np.abs(roughness - ref_roughness) * 255
            failures = np.flatnonzero((colour_error > 1) | (rough_error > 1))
            raw[method] = actual
            row["methods"].append(
                {
                    "method": method,
                    "seconds": seconds,
                    "colour_max_lsb": float(colour_error.max()),
                    "roughness_max_lsb": float(rough_error.max()),
                    "failed_pixels": failures.tolist(),
                    "colour_errors_lsb": colour_error.tolist(),
                    "roughness_errors_lsb": rough_error.tolist(),
                }
            )
        np.savez_compressed(output / f"{name}.npz", **raw)
        rows.append(row)
        print(
            name,
            "reference",
            order,
            [
                (m["method"], len(m["failed_pixels"]), round(m["colour_max_lsb"], 3))
                for m in row["methods"]
            ],
            flush=True,
        )
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output = args.output.resolve()
    root = Path(__file__).resolve().parents[3]
    if args.output.is_relative_to(root) or not args.output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("output must be outside the checkout in system temp")
    args.output.mkdir(parents=True, exist_ok=True)
    points = np.random.default_rng(781).uniform(-10, 10, (4096, 2))
    tail = float(np.max(np.abs(point_field(points, 3) - point_field(points, 4))))
    if tail > 1e-12:
        raise ValueError("reference tail control failed")
    report = {
        "scope": "CPU material parameter filters; no native BRDF or Place acceptance",
        "colour_limit_srgb_lsb": 1,
        "roughness_limit_255_steps": 1,
        "tail_max_absolute": tail,
        "cases": compare(args.output),
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (Path(__file__), Path(__file__).with_name("asphalt_field.py"))
        },
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
