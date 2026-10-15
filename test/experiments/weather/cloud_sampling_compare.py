"""Compare view quadrature against the same declared cloud field and lighting."""

import argparse
import json
import time
from dataclasses import replace
from pathlib import Path

import numpy as np
from cloud_comparison import view_rays
from cloud_field import Layer, segments, sphere_roots
from cloud_radiance import radiance


def cohort(bearing=240.0):
    x, y = np.meshgrid(np.arange(80, 1280, 160) + 0.5, np.arange(236, 386) + 0.5)
    direction = view_rays(
        1280, 720, bearing, -0.5, 33.97, pixel_centers=np.stack([x, y], axis=-1)
    )
    cosine = direction[..., 1].astype(np.float32).astype(float)
    azimuth = np.arctan2(direction[..., 2], direction[..., 0]).astype(np.float32)
    horizontal = np.sqrt(1.0 - cosine * cosine)
    return np.stack(
        [
            horizontal * np.cos(azimuth.astype(float)),
            cosine,
            horizontal * np.sin(azimuth.astype(float)),
        ],
        axis=-1,
    )


def errors(actual, reference):
    difference = actual - reference
    return {
        "rgb_rmse": float(np.sqrt(np.mean(difference[..., :3] ** 2))),
        "rgb_p99_abs": float(np.quantile(np.abs(difference[..., :3]), 0.99)),
        "rgb_max_abs": float(np.abs(difference[..., :3]).max()),
        "transmittance_rmse": float(np.sqrt(np.mean(difference[..., 3] ** 2))),
        "transmittance_max_abs": float(np.abs(difference[..., 3]).max()),
        "vertical_rgb_residual_rmse": float(
            np.sqrt(np.mean(np.diff(difference[..., :3], axis=0) ** 2))
        ),
    }


def evaluate(origin, direction, layer, seconds, **method):
    near, _, hit = sphere_roots(origin, direction, 0.0, layer.radius_m)
    maximum = np.where(hit & (near >= 0), near, np.inf)
    result = radiance(
        origin,
        direction,
        layer,
        maximum,
        seconds=seconds,
        **method,
    )
    assert np.isfinite(result).all() and (result >= 0).all()
    assert (result[..., 3] <= 1).all()
    return result


def methods():
    result = {
        f"n{count}-w{warp}": {"view_samples": count, "warp": warp}
        for count in (16, 32, 64)
        for warp in (1, 2)
    }
    result.update(
        {
            f"n{count}-w{warp}-beer": {
                "view_samples": count,
                "warp": warp,
                "source_centroid": True,
            }
            for count in (16, 32)
            for warp in (1, 2)
        }
    )
    for count in (16, 28, 32):
        result[f"n{count}-w2-gauss3"] = {
            "view_samples": count,
            "warp": 2,
            "light_samples": 3,
            "light_gauss3": True,
        }
    return result


def run_case(case, layer, reference_counts, reference_light_samples, baseline, output):
    name, height, seconds, east, bearing = case
    native_height = float(
        np.float32((np.float32(6360.0 + height * 0.001) - np.float32(6360.0)) * 1000)
    )
    origin = np.array([east, native_height, 0.0])
    direction = cohort(bearing)
    spans = segments(origin, direction, layer)
    intervals = (spans[..., 1] > spans[..., 0]).sum(axis=-1)
    configurations = {
        f"reference-{n}": {"view_samples": n, "light_samples": reference_light_samples}
        for n in reference_counts
    } | methods()
    values = {}
    for key, configuration in configurations.items():
        started = time.perf_counter()
        values[key] = evaluate(origin, direction, layer, seconds, **configuration)
        print(name, key, f"{time.perf_counter() - started:.2f}s", flush=True)
    reference = values[f"reference-{reference_counts[-1]}"]
    record = {
        "case": name,
        "origin_m": origin.tolist(),
        "seconds": seconds,
        "bearing_deg": bearing,
        "rays": int(intervals.size),
        "reference_refinement": errors(
            values[f"reference-{reference_counts[0]}"], reference
        ),
        "methods": {},
    }
    for key, configuration in methods().items():
        difference = values[key] - reference
        row = errors(values[key], reference)
        count = configuration["view_samples"]
        row["view_density_upper"] = {
            "sum": int(intervals.sum() * count),
            "max": int(intervals.max() * count),
        }
        row["cloud_density_per_ray_upper"] = (
            2 * count * (1 + 2 * configuration.get("light_samples", 6))
        )
        if name == "below":
            baseline[key] = difference
        elif name in ("wind-tick", "camera-tick") and key in baseline:
            row["temporal_error_change_rgb_rmse"] = float(
                np.sqrt(np.mean((difference[..., :3] - baseline[key][..., :3]) ** 2))
            )
        record["methods"][key] = row
    np.savez(output / f"{name}.npz", **values)
    return record


def main():
    cases = [
        ("below", 1772.0, 37.0, 0.0, 240.0),
        ("wind-tick", 1772.0, 37.0 + 1.0 / 30.0, 0.0, 240.0),
        ("camera-tick", 1772.0, 37.0, 1.0, 240.0),
        ("inside", 2800.0, 37.0, 0.0, 240.0),
        ("above", 4000.0, 37.0, 0.0, 240.0),
        ("wind-minute", 1772.0, 97.0, 0.0, 240.0),
        ("turned", 1772.0, 37.0, 0.0, 245.0),
    ]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--references", type=int, nargs=2, default=[512, 1024])
    parser.add_argument("--reference-light-samples", type=int, default=6)
    parser.add_argument("--cases", nargs="+", choices=[case[0] for case in cases])
    args = parser.parse_args()
    if not 128 <= args.references[0] < args.references[1] <= 4096:
        parser.error("require two increasing reference counts in [128,4096]")
    if not 1 <= args.reference_light_samples <= 128:
        parser.error("reference light samples must be in [1,128]")
    args.output.mkdir(parents=True, exist_ok=True)
    records, baseline = [], {}
    for case in cases:
        if args.cases and case[0] not in args.cases:
            continue
        records.append(
            run_case(
                case,
                replace(Layer(), cover=0.65),
                args.references,
                args.reference_light_samples,
                baseline,
                args.output,
            )
        )
        (args.output / "results.json").write_text(
            json.dumps(
                {
                    "resolution": [1280, 720],
                    "cohort_columns": list(range(80, 1280, 160)),
                    "cohort_rows": [236, 385],
                    "reference_light_samples": args.reference_light_samples,
                    "reference_counts": args.references,
                    "cases": records,
                    "methods": methods(),
                    "scope": "Float64 view/source quadrature with the same declared field; no pixel-area, exact integral, full-image, GPU-time, runtime or budget acceptance. Work values are bounds, not measured GPU execution.",
                },
                indent=2,
            )
        )


if __name__ == "__main__":
    main()
