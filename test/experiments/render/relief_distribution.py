"""Split ideal normal groups before choosing a bounded procedural shading model."""

import argparse
import hashlib
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
from facade_coating_raster import ROOT
from relief_shading import ANGLES, ROUGHNESS
from relief_shading_reference import direction, normals, specular_from_dots

CAPACITIES = (9, 17, 33, 65, 129)
MAX_PEAK_ERROR = 0.02
RMS_PEAK_ERROR = 0.005


def group(unit, weights, indices, work):
    values, local_weights = unit[indices], weights[indices]
    area = local_weights.sum()
    mean = local_weights @ values / area
    centred = values - mean
    covariance = (centred.T * local_weights) @ centred / area
    eigenvalues, eigenvectors = np.linalg.eigh(covariance)
    work["group_statistics"] += 1
    work["point_visits"] += len(indices)
    work["indexed_array_copies"] += 2
    work["indexed_copy_bytes"] += values.nbytes + local_weights.nbytes
    return {
        "indices": indices,
        "area": area,
        "normal": mean / np.linalg.norm(mean),
        "score": max(eigenvalues[-1], 0),
        "axis": eigenvectors[:, -1],
    }


def partition(gradient, weights, strategy="area"):
    unit = normals(gradient)
    labels = (
        (np.sign(gradient[:, 0]).astype(int) + 1) * 3
        + np.sign(gradient[:, 1]).astype(int)
        + 1
    )
    work = {
        "group_statistics": 0,
        "point_visits": 0,
        "indexed_array_copies": 0,
        "indexed_copy_bytes": 0,
        "sorts": 0,
        "sorted_elements": 0,
    }
    groups = []
    for label in range(9):
        indices = np.flatnonzero((labels == label) & (weights > 0))
        if len(indices) and weights[indices].sum() > 1e-15:
            groups.append(group(unit, weights, indices, work))
    models = {}
    for capacity in CAPACITIES:
        while capacity > 9 and len(groups) < capacity:
            index = max(
                range(len(groups)),
                key=lambda i: (
                    groups[i]["score"]
                    * (groups[i]["area"] if strategy == "area" else 1)
                ),
            )
            old = groups[index]
            if old["score"] <= 1e-15 or len(old["indices"]) < 2:
                break
            indices = old["indices"]
            order = np.argsort(unit[indices] @ old["axis"], kind="stable")
            ordered = indices[order]
            split = (
                int(np.searchsorted(np.cumsum(weights[ordered]), old["area"] / 2)) + 1
            )
            split = min(max(split, 1), len(indices) - 1)
            work["sorts"] += 1
            work["sorted_elements"] += len(indices)
            groups[index] = group(unit, weights, ordered[:split], work)
            groups.append(group(unit, weights, ordered[split:], work))
        lobes = np.array([g["normal"] for g in groups])
        coverage = np.array([g["area"] for g in groups])
        np.testing.assert_allclose(coverage.sum(), 1, atol=1e-12, rtol=0)
        np.testing.assert_allclose(np.linalg.norm(lobes, axis=1), 1, atol=1e-12, rtol=0)
        models[capacity] = (lobes, coverage, work.copy())
    return models


def response(lobes, coverage, roughness, lights, view):
    nl = np.maximum(lobes @ lights.T, 0)
    nv = np.maximum(lobes @ view, 1e-6)[:, None]
    half = lights + view
    half /= np.linalg.norm(half, axis=1)[:, None]
    nh = np.maximum(lobes @ half.T, 0)
    vh = np.maximum(half @ view, 0)[None]
    return coverage @ specular_from_dots(roughness, nl, nv, nh, vh)


def response_bounds(lobes, coverage, roughness, lights, view, dot_error=3e-7):
    """Propagate the separately checked float32 dot uncertainty through monotone GGX."""
    nl, nv = lobes @ lights.T, (lobes @ view)[:, None]
    half = lights + view
    half /= np.linalg.norm(half, axis=1)[:, None]
    nh, vh = lobes @ half.T, (half @ view)[None]
    lower = specular_from_dots(
        roughness,
        np.maximum(nl - dot_error, 0),
        np.maximum(nv + dot_error, 1e-6),
        np.maximum(nh - dot_error, 0),
        np.maximum(vh + dot_error, 0),
    )
    upper = specular_from_dots(
        roughness,
        np.maximum(nl + dot_error, 0),
        np.maximum(nv - dot_error, 1e-6),
        np.maximum(nh + dot_error, 0),
        np.maximum(vh - dot_error, 0),
    )
    return coverage @ lower, coverage @ upper


def curves(lobes, coverage):
    return np.array(
        [
            response(
                lobes,
                coverage,
                roughness,
                np.array([direction(angle, azimuth) for angle in ANGLES]),
                direction(view),
            )
            for roughness in ROUGHNESS
            for view in (0, 50)
            for azimuth in (0, 90)
        ]
    )


def compare(source, output, strategy):
    report = json.loads((source / "report.json").read_text())
    for name, expected in report["source_sha256"].items():
        if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f"reference source changed: {name}")
    rows = []
    for case in report["cases"]:
        path = source / f"{case['name']}.npz"
        with np.load(path) as raw:
            gradient, weights, reference = (
                raw["gradient"],
                raw["weights"],
                raw["response"],
            )
        tracemalloc.start()
        started = time.perf_counter()
        models = partition(gradient, weights, strategy)
        seconds = time.perf_counter() - started
        _, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        row = {
            "name": case["name"],
            "input_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "build_seconds": seconds,
            "traced_build_peak_bytes": peak,
            "methods": [],
        }
        for capacity, (lobes, coverage, work) in models.items():
            actual = curves(lobes, coverage)
            if capacity == 9:
                np.testing.assert_allclose(actual, reference[:, -1], atol=1e-11, rtol=0)
            if case["name"] in ("flat-interior", "zero-depth", "joint-0.0002"):
                np.testing.assert_allclose(actual, reference[:, 0], atol=1e-11, rtol=0)
                assert len(lobes) == 1
            delta = actual - reference[:, 0]
            scale = np.max(np.abs(reference[:, 0]), axis=1)
            maximum = np.max(np.abs(delta), axis=1) / scale
            rms = np.sqrt(np.mean(delta * delta, axis=1)) / scale
            failures = np.flatnonzero(
                (maximum > MAX_PEAK_ERROR) | (rms > RMS_PEAK_ERROR)
            )
            row["methods"].append(
                {
                    "capacity": capacity,
                    "lobes": len(lobes),
                    "max_peak_error": float(maximum.max()),
                    "rms_peak_error": float(rms.max()),
                    "failed_curves": failures.tolist(),
                    "work": work,
                }
            )
            np.savez_compressed(
                output / f"{case['name']}-{capacity}.npz",
                lobes=lobes,
                coverage=coverage,
                response=actual,
                reference=reference[:, 0],
                mean_gradient_normal=normals((weights @ gradient)[None])[0],
            )
        rows.append(row)
        print(
            case["name"],
            [(m["capacity"], len(m["failed_curves"])) for m in row["methods"]],
            flush=True,
        )
    return {
        "scope": "Ideal offline normal distribution; no bounded runtime statistics claim",
        "allocation_strategy": strategy,
        "maximum_peak_error_limit": MAX_PEAK_ERROR,
        "rms_peak_error_limit": RMS_PEAK_ERROR,
        "reference_report_sha256": hashlib.sha256(
            (source / "report.json").read_bytes()
        ).hexdigest(),
        "cases": rows,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--strategy", choices=("area", "angular"), default="area")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report = compare(args.reference, args.output, args.strategy)
    report["source_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    for capacity in CAPACITIES:
        methods = [
            next(m for m in c["methods"] if m["capacity"] == capacity)
            for c in report["cases"]
        ]
        print(
            capacity,
            "failed curves",
            sum(len(m["failed_curves"]) for m in methods),
            "max relative",
            max(m["max_peak_error"] for m in methods),
            flush=True,
        )


if __name__ == "__main__":
    main()
