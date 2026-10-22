"""Stop the finite-covariance candidate at the unchanged known counterexample."""

import argparse
import hashlib
import json
import time
import tracemalloc
from collections import Counter
from pathlib import Path

import numpy as np
from facade_coating_raster import ROOT
from relief_distribution import MAX_PEAK_ERROR, RMS_PEAK_ERROR
from relief_finite import curves
from relief_finite_kernel import footprint, transform
from relief_kernel_covariance import kernel_covariance, residual_hessian, response
from relief_shading import ANGLES, ROUGHNESS, cases
from relief_shading_reference import direction, normals


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def evaluate(model, step):
    values, work, error = [], Counter(), 0.0
    for roughness in ROUGHNESS:
        for view in (0, 50):
            for azimuth in (0, 90):
                lights = np.array([direction(a, azimuth) for a in ANGLES])
                value, counted, estimate = response(
                    model, roughness, lights, direction(view), step=step
                )
                values.append(value)
                work.update(counted)
                error = max(error, estimate)
    return np.asarray(values), dict(work), error


def errors(actual, expected):
    scale = np.max(abs(expected), axis=1)
    difference = actual - expected
    maximum = np.max(abs(difference), axis=1) / scale
    rms = np.sqrt(np.mean(difference * difference, axis=1)) / scale
    return {
        "maximum_peak_error": maximum.tolist(),
        "rms_peak_error": rms.tolist(),
        "failed_curves": np.flatnonzero(
            (maximum > MAX_PEAK_ERROR) | (rms > RMS_PEAK_ERROR)
        ).tolist(),
        "negative_response_samples": int(np.count_nonzero(actual < 0)),
        "negative_response_curves": np.flatnonzero(np.any(actual < 0, axis=1)).tolist(),
    }


def diagnosis(model, actual, expected):
    peak = np.max(abs(expected), axis=1)
    curve, light = np.unravel_index(
        np.argmax(abs(actual - expected) / peak[:, None]), actual.shape
    )
    r, remainder = divmod(curve, 4)
    view, azimuth = divmod(remainder, 2)
    roughness, view, azimuth = ROUGHNESS[r], (0, 50)[view], (0, 90)[azimuth]
    lights, v = np.array([direction(ANGLES[light], azimuth)]), direction(view)
    vertices = model[2][0][0]
    tr = transform(roughness, lights, v)
    mass, centre, covariance, _, error = kernel_covariance(vertices, tr)
    base, hessian = residual_hessian(centre, roughness, lights, v, tr[-1], 1e-3)
    return {
        "curve": int(curve),
        "light_index": int(light),
        "light_angle": float(ANGLES[light]),
        "roughness": float(roughness),
        "view": view,
        "azimuth": azimuth,
        "actual": float(actual[curve, light]),
        "reference": float(expected[curve, light]),
        "raw_nv_range": [
            float((normals(vertices) @ v).min()),
            float((normals(vertices) @ v).max()),
        ],
        "raw_nl_range": [
            float((normals(vertices) @ lights[0]).min()),
            float((normals(vertices) @ lights[0]).max()),
        ],
        "mass": mass.tolist(),
        "centre": centre.tolist(),
        "covariance": covariance.tolist(),
        "base_residual": base.tolist(),
        "hessian": hessian.tolist(),
        "radial_quadrature_covariance_error_estimate": error,
    }


def run(source, output):
    manifest = json.loads((source / "report.json").read_text())
    for name, expected in manifest["source_sha256"].items():
        if digest(ROOT / name) != expected:
            raise ValueError(f"unchanged reference source required: {name}")
    declared = {case["name"]: case for case in cases()}
    if (
        len(manifest["cases"]) != 24
        or {case["name"] for case in manifest["cases"]} != declared.keys()
    ):
        raise ValueError("all original 24 reference footprints must remain declared")
    name = "sheared-corner-0.003"
    with np.load(source / (name + ".npz"), allow_pickle=False) as raw:
        expected = raw["response"][:, 0]
    began = time.perf_counter()
    model = footprint(declared[name], 1)
    build_seconds = time.perf_counter() - began
    baseline = curves(model)
    estimates, reports = {}, {}
    for step in (1e-3, 3e-4, 1e-4):
        began = time.perf_counter()
        values, work, error = evaluate(model, step)
        reports[str(step)] = dict(
            errors(values, expected),
            work=work,
            evaluate_seconds=time.perf_counter() - began,
            maximum_radial_covariance_error_estimate=error,
        )
        estimates[str(step)] = values
    tracemalloc.start()
    traced, counted, _ = evaluate(model, 1e-3)
    peak = tracemalloc.get_traced_memory()[1]
    tracemalloc.stop()
    np.testing.assert_array_equal(traced, estimates["0.001"])
    assert counted == reports["0.001"]["work"]
    np.savez_compressed(
        output / "curves.npz", reference=expected, baseline=baseline, **estimates
    )
    report = {
        "scope": "First known counterexample, 12 of 288 curves. All original cases/limits retained; one failed curve suffices to reject the candidate.",
        "case": name,
        "reference_report_sha256": digest(source / "report.json"),
        "reference_npz_sha256": digest(source / (name + ".npz")),
        "maximum_peak_error_limit": MAX_PEAK_ERROR,
        "rms_peak_error_limit": RMS_PEAK_ERROR,
        "baseline": errors(baseline, expected),
        "methods": reports,
        "model_work": model[3],
        "build_seconds": build_seconds,
        "query_python_traced_peak_bytes": peak,
        "diagnosis": diagnosis(model, estimates["0.001"], expected),
        "source_sha256": {
            str(p.relative_to(ROOT)): digest(p)
            for p in (
                Path(__file__).resolve(),
                Path(__file__).with_name("relief_kernel_covariance.py").resolve(),
                Path(__file__).with_name("relief_covariance_reference.py").resolve(),
            )
        },
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    result = run(args.reference, args.output)
    print(
        json.dumps(
            {
                "case": result["case"],
                "failed_curves": {
                    k: v["failed_curves"] for k, v in result["methods"].items()
                },
                "decision": "reject"
                if result["methods"]["0.001"]["failed_curves"]
                or result["methods"]["0.001"]["negative_response_samples"]
                else "counterexample passed; complete comparison still required",
            }
        )
    )
