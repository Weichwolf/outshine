"""Compare exact lit support with the unchanged 24-footprint GGX reference (#931)."""

import argparse
import hashlib
import json
import tempfile
import time
import tracemalloc
from collections import Counter
from pathlib import Path

import numpy as np
from facade_coating_raster import ROOT
from relief_distribution import MAX_PEAK_ERROR, RMS_PEAK_ERROR
from relief_finite import compare as baseline
from relief_finite_kernel import footprint
from relief_lit_kernel import response
from relief_shading import ANGLES, ROUGHNESS, cases
from relief_shading_reference import direction


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def evaluate(model):
    values, work = [], Counter()
    for roughness in ROUGHNESS:
        for view in (0, 50):
            for azimuth in (0, 90):
                lights = np.array([direction(angle, azimuth) for angle in ANGLES])
                value, measured = response(model, roughness, lights, direction(view))
                values.append(value)
                work.update(measured)
    return np.asarray(values), dict(work)


def compare(source, output):
    controls = output / "baseline"
    controls.mkdir()
    old = baseline(source, controls)
    counts = [sum(len(c["methods"][i]["failed_curves"]) for c in old) for i in (0, 1)]
    if counts != [14, 10]:
        raise ValueError(f"unchanged negative controls changed: {counts}")
    inputs = {case["name"]: case for case in cases()}
    rows = []
    for control in old:
        name = control["name"]
        with np.load(source / f"{name}.npz") as raw:
            reference = raw["response"][:, 0]
        started = time.perf_counter()
        model = footprint(inputs[name], 1)
        build_seconds = time.perf_counter() - started
        started = time.perf_counter()
        actual, work = evaluate(model)
        evaluate_seconds = time.perf_counter() - started
        # Allocation tracing changes timing; measure it in a separate identical replay.
        tracemalloc.start()
        traced, traced_work = evaluate(model)
        _, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        np.testing.assert_array_equal(actual, traced)
        if work != traced_work:
            raise ValueError("allocation tracing changed the counted work")
        if not np.all(np.isfinite(actual)) or np.any(actual < 0):
            raise ValueError(f"invalid lit-support response: {name}")
        if name in ("flat-interior", "zero-depth", "joint-0.0002") or name.startswith(
            "motion-"
        ):
            np.testing.assert_allclose(actual, reference, rtol=0, atol=1e-11)
        delta = actual - reference
        scale = np.max(np.abs(reference), axis=1)
        maximum = np.max(np.abs(delta), axis=1) / scale
        rms = np.sqrt(np.mean(delta * delta, axis=1)) / scale
        failed = np.flatnonzero((maximum > MAX_PEAK_ERROR) | (rms > RMS_PEAK_ERROR))
        rows.append(
            {
                "name": name,
                "input_sha256": digest(source / f"{name}.npz"),
                "failed_curves": failed.tolist(),
                "curve_max_peak_error": maximum.tolist(),
                "curve_rms_peak_error": rms.tolist(),
                "work": work,
                "model_work": model[-1],
                "build_seconds": build_seconds,
                "evaluate_seconds": evaluate_seconds,
                "traced_evaluate_peak_bytes": peak,
            }
        )
        np.savez_compressed(
            output / f"{name}.npz", response=actual, reference=reference
        )
        print(
            name,
            "lit support",
            len(failed),
            "failed curves",
            float(maximum.max()),
            flush=True,
        )
    report = {
        "scope": "CPU component response only; original guards, no native/runtime/Place admission",
        "maximum_peak_error_limit": MAX_PEAK_ERROR,
        "rms_peak_error_limit": RMS_PEAK_ERROR,
        "reference_report_sha256": digest(source / "report.json"),
        "baseline": old,
        "baseline_failed_curves": counts,
        "cases": rows,
        "failed_curves": sum(len(row["failed_curves"]) for row in rows),
        "source_sha256": {
            str(path.relative_to(ROOT)): digest(path)
            for path in (
                Path(__file__).resolve(),
                Path(__file__).with_name("relief_lit_kernel.py").resolve(),
                Path(__file__).with_name("relief_finite_kernel.py").resolve(),
                Path(__file__).with_name("relief_shading_reference.py").resolve(),
            )
        },
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or args.output.resolve().is_relative_to(ROOT):
        parser.error("private outputs belong in system temp, outside the checkout")
    args.output.mkdir(parents=True)
    report = compare(args.reference, args.output)
    print(
        "COMPLETE",
        report["failed_curves"],
        "/288 failed curves; no native admission",
        flush=True,
    )


if __name__ == "__main__":
    main()
