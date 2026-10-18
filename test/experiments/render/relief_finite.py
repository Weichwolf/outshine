"""Compare finite GGX corner responses against the original 288 light curves."""

import argparse
import hashlib
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
from facade_coating_raster import ROOT
from relief_distribution import MAX_PEAK_ERROR, RMS_PEAK_ERROR
from relief_finite_kernel import finite_response, footprint, kernel_moments, transform
from relief_shading import ANGLES, ROUGHNESS, cases
from relief_shading_reference import direction, triangle_rule
from shapely.geometry import Point, Polygon


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def kernel_quadrature(vertices, roughness, half, order):
    """Stream four midpoint subtriangles per original fan triangle."""
    result = np.zeros((len(half), 3))
    for index in range(1, len(vertices) - 1):
        a, b, c = vertices[[0, index, index + 1]]
        ab, bc, ca = (a + b) / 2, (b + c) / 2, (c + a) / 2
        for triangle in ((a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca)):
            points, weights = triangle_rule(Polygon(triangle), order)
            quadratic = (
                1
                + np.sum(points * points, axis=1)[:, None]
                + (roughness**4 - 1) * (half[:, 2][None] - points @ half[:, :2].T) ** 2
            )
            weighted = weights[:, None] * roughness**4 / (np.pi * quadratic**2)
            result += np.column_stack([weighted.sum(axis=0), (points.T @ weighted).T])
    return result


def kernel_controls():
    """Independent triangle integration of the original quadratic, not edge sums."""
    polygons = [
        np.array([[-1.0, -1], [1, -1], [1, 1], [-1, 1]]),
        np.array([[-1.0, -0.3], [0.7, -0.4], [1, 0.6], [-0.6, 0.9]]),
        np.array([[0.6, 0.3], [0.63, 0.31], [0.61, 0.36]]),
    ]
    lights = np.array(
        [direction(a, az) for az in (0, 90) for a in (-80, -15, 0, 65, 80)]
    )
    records = []
    for index, vertices in enumerate(polygons):
        for roughness in ROUGHNESS:
            view = direction(50)
            transformed = transform(roughness, lights, view)
            mass, centroid = kernel_moments(vertices, transformed)
            previous = None
            for order in (128, 256, 512):
                current = kernel_quadrature(vertices, roughness, transformed[-1], order)
                if previous is not None and np.max(np.abs(current - previous)) < 1e-10:
                    break
                previous = current
            else:
                raise ValueError("independent kernel quadrature did not converge")
            expected_mass, expected_first = current[:, 0], current[:, 1:].T
            np.testing.assert_allclose(mass, expected_mass, rtol=2e-9, atol=2e-11)
            np.testing.assert_allclose(
                centroid, expected_first.T / expected_mass[:, None], rtol=0, atol=2e-9
            )
            shape = Polygon(vertices).buffer(2e-9)
            if not all(shape.covers(Point(p)) for p in centroid):
                raise ValueError("kernel centroid lies outside its convex polygon")
            # Reversal changes signed mass; unsupported clockwise input must fail.
            try:
                kernel_moments(vertices[::-1], transformed)
            except ValueError:
                pass
            else:
                raise ValueError("clockwise kernel polygon was accepted")
            # Swap the two tangent axes, preserving z and positive orientation.
            permutation = [1, 0, 2]
            swapped = transform(roughness, lights[:, permutation], view[permutation])
            swap_mass, swap_centroid = kernel_moments(vertices[::-1, ::-1], swapped)
            np.testing.assert_allclose(swap_mass, mass, rtol=2e-9, atol=2e-11)
            np.testing.assert_allclose(
                swap_centroid[:, ::-1], centroid, rtol=0, atol=2e-9
            )
            records.append(
                {
                    "polygon": index,
                    "roughness": roughness,
                    "order": order,
                    "mass_absolute_error": float(np.max(np.abs(mass - expected_mass))),
                    "centroid_absolute_error": float(
                        np.max(
                            np.abs(centroid - expected_first.T / expected_mass[:, None])
                        )
                    ),
                }
            )
    return records


def curves(model):
    return np.array(
        [
            finite_response(
                model,
                roughness,
                np.array([direction(a, azimuth) for a in ANGLES]),
                direction(view),
            )
            for roughness in ROUGHNESS
            for view in (0, 50)
            for azimuth in (0, 90)
        ]
    )


def compare(source, output):
    reference_report = json.loads((source / "report.json").read_text())
    for name, expected in reference_report["source_sha256"].items():
        if digest(ROOT / name) != expected:
            raise ValueError(f"reference source changed: {name}")
    inputs = {case["name"]: case for case in cases()}
    names = [case["name"] for case in reference_report["cases"]]
    if len(names) != 24 or set(names) != inputs.keys():
        raise ValueError("reference must retain all 24 declared footprints")
    rows = []
    for reference_case in reference_report["cases"]:
        name = reference_case["name"]
        with np.load(source / f"{name}.npz") as raw:
            reference = raw["response"][:, 0]
            mean_gradient = raw["weights"] @ raw["gradient"]
        row = {
            "name": name,
            "input_sha256": digest(source / f"{name}.npz"),
            "methods": [],
        }
        for tiles in (1, 4):
            tracemalloc.start()
            started = time.perf_counter()
            model = footprint(inputs[name], tiles)
            build_seconds = time.perf_counter() - started
            _, build_peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
            gradient, coverage, corners, work = model
            integrated_gradient = coverage @ gradient
            for vertices, density in corners:
                polygon = Polygon(vertices)
                integrated_gradient += (
                    density * polygon.area * np.asarray(polygon.centroid.coords[0])
                )
            np.testing.assert_allclose(
                integrated_gradient, mean_gradient, rtol=0, atol=1e-10
            )
            started = time.perf_counter()
            actual = curves(model)
            evaluate_seconds = time.perf_counter() - started
            light_evaluations = len(ROUGHNESS) * 2 * 2 * len(ANGLES)
            work["constant_brdf_evaluations"] = light_evaluations * len(coverage)
            work["residual_brdf_evaluations"] = light_evaluations * len(corners)
            work["analytic_edge_evaluations"] = light_evaluations * work["edges"]
            work["kernel_transform_systems"] = light_evaluations if corners else 0
            if not np.all(np.isfinite(actual)) or np.any(actual < 0):
                raise ValueError("invalid finite response")
            if name in (
                "flat-interior",
                "zero-depth",
                "joint-0.0002",
            ) or name.startswith("motion-"):
                np.testing.assert_allclose(actual, reference, rtol=0, atol=1e-11)
            delta = actual - reference
            scale = np.max(np.abs(reference), axis=1)
            maximum = np.max(np.abs(delta), axis=1) / scale
            rms = np.sqrt(np.mean(delta * delta, axis=1)) / scale
            failures = np.flatnonzero(
                (maximum > MAX_PEAK_ERROR) | (rms > RMS_PEAK_ERROR)
            )
            row["methods"].append(
                {
                    "tiles_per_corner": tiles,
                    "work": work,
                    "build_seconds": build_seconds,
                    "traced_build_peak_bytes": build_peak,
                    "evaluate_seconds": evaluate_seconds,
                    "failed_curves": failures.tolist(),
                    "max_peak_error": float(maximum.max()),
                    "rms_peak_error": float(rms.max()),
                    "curve_max_peak_error": maximum.tolist(),
                    "curve_rms_peak_error": rms.tolist(),
                }
            )
            np.savez_compressed(
                output / f"{name}-{tiles}.npz", response=actual, reference=reference
            )
        rows.append(row)
        print(
            name,
            [
                (m["tiles_per_corner"], len(m["failed_curves"]), m["max_peak_error"])
                for m in row["methods"]
            ],
            flush=True,
        )
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    controls = kernel_controls()
    report = {
        "scope": "Ideal finite slope polygons in float64, not native/runtime/Place evidence",
        "maximum_peak_error_limit": MAX_PEAK_ERROR,
        "rms_peak_error_limit": RMS_PEAK_ERROR,
        "reference_report_sha256": digest(args.reference / "report.json"),
        "kernel_controls": controls,
        "cases": compare(args.reference, args.output),
        "source_sha256": {
            str(p.relative_to(ROOT)): digest(p)
            for p in (
                Path(__file__).resolve(),
                Path(__file__).with_name("relief_finite_kernel.py").resolve(),
                Path(__file__).with_name("relief_distribution.py").resolve(),
                Path(__file__).with_name("facade_footprint.py").resolve(),
            )
        },
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
