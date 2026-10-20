"""Compare one bounded adaptive-range model with the unchanged hard footprint."""

import argparse
import hashlib
import json
import platform
import time
import tracemalloc
from collections import Counter
from pathlib import Path

import numpy as np
from compare_relief_covariance import errors
from facade_coating_raster import ROOT
from relief_finite import curves
from relief_finite_kernel import footprint
from relief_residual_range import response
from relief_shading import ANGLES, ROUGHNESS, cases
from relief_shading_reference import direction


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(reference, output):
    manifest = json.loads((reference / "report.json").read_text())
    for name, expected in manifest["source_sha256"].items():
        if digest(ROOT / name) != expected:
            raise ValueError(f"original reference source changed: {name}")
    declared = {c["name"]: c for c in cases()}
    if (
        len(manifest["cases"]) != 24
        or {c["name"] for c in manifest["cases"]} != declared.keys()
    ):
        raise ValueError("all original 24 reference footprints must remain declared")
    name = "sheared-corner-0.003"
    archive = reference / f"{name}.npz"
    if (
        digest(archive)
        != "d7b305c0cbac4ca0a6b7a8ebc7c935c2c3931d79e91db19bdec22b5f365d9397"
    ):
        raise ValueError("unchanged independently converged original archive required")
    with np.load(archive, allow_pickle=False) as data:
        expected = data["response"][:, 0]
    model = footprint(declared[name], 1)
    baseline_began = time.perf_counter()
    baseline = curves(model)
    baseline_seconds = time.perf_counter() - baseline_began
    actual, intervals, leaves, met = [], [], [], []
    total_work, history = Counter(), []
    active_peak = 0
    began = time.perf_counter()
    for roughness in ROUGHNESS:
        for view_angle in (0, 50):
            for azimuth in (0, 90):
                values, bounds, counts, statuses = [], [], [], []
                for index, angle in enumerate(ANGLES):
                    trace = (
                        history
                        if (
                            roughness == 0.2
                            and view_angle == 50
                            and azimuth == 90
                            and angle == -35
                        )
                        else None
                    )
                    value, interval, work = response(
                        model,
                        roughness,
                        direction(angle, azimuth),
                        direction(view_angle),
                        history=trace,
                    )
                    values.append(value)
                    bounds.append(interval)
                    counts.append(work["leaves"])
                    statuses.append(work["model_tolerance_met"])
                    active_peak = max(active_peak, work["active_numeric_array_bytes"])
                    total_work.update(
                        {
                            k: v
                            for k, v in work.items()
                            if k
                            not in (
                                "leaves",
                                "model_tolerance_met",
                                "active_numeric_array_bytes",
                            )
                        }
                    )
                actual.append(values)
                intervals.append(bounds)
                leaves.append(counts)
                met.append(statuses)
                print(
                    f"r={roughness} view={view_angle} azimuth={azimuth}: {sum(statuses)}/{len(ANGLES)} range criteria met",
                    flush=True,
                )
    elapsed = time.perf_counter() - began
    actual, intervals, leaves, met = (
        np.asarray(actual),
        np.asarray(intervals),
        np.asarray(leaves),
        np.asarray(met),
    )
    below = expected < intervals[:, :, 0] - 1e-11
    above = expected > intervals[:, :, 1] + 1e-11
    light, view = direction(-35, 90), direction(50)
    tracemalloc.start()
    replay, replay_interval, replay_work = response(model, 0.2, light, view)
    peak = tracemalloc.get_traced_memory()[1]
    live = sum(s.count for s in tracemalloc.take_snapshot().statistics("filename"))
    tracemalloc.stop()
    pilot_index = int(np.flatnonzero(ANGLES == -35)[0])
    if replay != actual[3, pilot_index] or not np.array_equal(
        replay_interval, intervals[3, pilot_index]
    ):
        raise ValueError("allocation replay changed numerical result")
    arrays = {
        "response": actual,
        "intervals": intervals,
        "leaves": leaves,
        "model_tolerance_met": met,
        "reference": expected,
        "baseline": baseline,
        "history": np.asarray(history),
    }
    report = {
        "scope": "12 hard-footprint curves, 780 queries of 18720 original queries; not full filter/device admission",
        "case": name,
        "relative_model_tolerance": 0.004,
        "leaf_limit": 64,
        "baseline": errors(baseline, expected),
        "adaptive": errors(actual, expected),
        "queries_met": int(met.sum()),
        "queries_unproved_at_budget": int((~met).sum()),
        "reference_outside_model_range_with_1e-11_numerical_control": int(
            np.count_nonzero(below | above)
        ),
        "maximum_control_excess": max(
            float((intervals[:, :, 0] - expected).max()),
            float((expected - intervals[:, :, 1]).max()),
            0.0,
        ),
        "work": dict(total_work),
        "maximum_live_leaves": int(leaves.max()),
        "maximum_query_active_numeric_array_bytes": active_peak,
        "retained_result_array_bytes": sum(a.nbytes for a in arrays.values()),
        "query_seconds": elapsed,
        "baseline_query_seconds": baseline_seconds,
        "allocation_pilot": {
            "python_traced_peak_bytes": peak,
            "traced_retained_allocation_blocks": live,
            "work": replay_work,
        },
        "original_archive_sha256": digest(archive),
        "reference_report_sha256": digest(reference / "report.json"),
        "environment": {
            "platform": platform.platform(),
            "python": platform.python_version(),
            "numpy": np.__version__,
        },
        "rounding_scope": "real-arithmetic residual enclosure; Float64/kernel mass rounding is checked against independent reference, not formally enclosed",
    }
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(output / "comparison.npz", **arrays)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.reference, args.output)
