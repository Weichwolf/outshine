"""Separate whole-box/corner cost and tightness with already held source metadata."""

import argparse
import gc
import json
import os
import platform
import subprocess
import tempfile
import time
from functools import partial
from pathlib import Path

import numpy as np
from allocation_measure import allocations
from capture_box_envelope import box_envelope, conditionally_inside
from capture_box_reference import ROOT, corner_envelope, submitted_inputs
from capture_support_reference import metadata_inputs


def equal(expected, actual):
    for key in expected:
        np.testing.assert_array_equal(expected[key], actual[key])


def measure(methods, output, scope):
    expected = {name: call() for name, call in methods.items()}
    np.testing.assert_array_equal(
        conditionally_inside(expected["box"]), conditionally_inside(expected["corners"])
    )
    report = {name: {"rounds": []} for name in methods}
    names = tuple(methods)
    for iteration in range(9):
        for name in names[iteration % 2 :] + names[: iteration % 2]:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            value = methods[name]()
            report[name]["rounds"].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            equal(expected[name], value)
            del value
    for name, call in methods.items():
        gc.collect()
        value, memory = allocations(output / f"{scope}-{name}.bin", call)
        equal(expected[name], value)
        report[name].update(memory)
        for clock in ("cpu", "wall"):
            report[name][clock + "_median_ms"] = 1000 * float(
                np.median([r[clock + "_s"] for r in report[name]["rounds"]])
            )
    box, corners = expected["box"], expected["corners"]
    gap = np.maximum(corners["lower"] - box["lower"], box["upper"] - corners["upper"])
    return {
        "methods": report,
        "max_box_excess_vs_corners": gap.max(0).tolist(),
        "conditionally_inside": int(conditionally_inside(box).sum()),
        "result_array_bytes": sum(
            v.nbytes for v in box.values() if isinstance(v, np.ndarray)
        ),
    }


def run(args):
    producers = [
        Path(__file__).parent / name
        for name in (
            "capture_box_cost.py",
            "capture_box_reference.py",
            "capture_box_envelope.py",
            "capture_source_support.py",
            "capture_support_reference.py",
            "vertex_projection/verify_envelope.py",
        )
    ]
    producers.append(ROOT / "test/experiments/buildings/allocation_measure.py")
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, producers)], text=True
    ):
        raise ValueError("Freeze the measured code before recording costs")
    output, temp = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    if output.is_relative_to(ROOT) or output == temp or not output.is_relative_to(temp):
        raise ValueError("Use a fresh explicit system-temp output")
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Measure at normal process priority")
    output.mkdir(parents=True, exist_ok=False)
    manifest, _, operands = submitted_inputs()
    _, arrays = metadata_inputs(
        ROOT / "test/outshine/data/render/building-field-composition"
    )
    low, high = arrays["source_least_m"], arrays["source_most_m"]
    rows, load = [], os.getloadavg()
    for scope, a, b in (
        ("cohort", low.min(0, keepdims=True), high.max(0, keepdims=True)),
        ("sources", low, high),
    ):
        methods = {
            "box": partial(
                box_envelope, a, b, *operands, positions_normal_or_zero=True
            ),
            "corners": partial(
                corner_envelope, a, b, *operands, positions_normal_or_zero=True
            ),
        }
        rows.append(
            {
                "scope": scope,
                "bounds": len(a),
                "input_endpoint_bytes": a.nbytes + b.nbytes,
                **measure(methods, output, scope),
            }
        )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "host": platform.platform(),
        "load_before": load,
        "load_after": os.getloadavg(),
        "submitted_npz_sha256": manifest["npz_sha256"],
        "rows": rows,
        "scope": "Held metadata Python CPU and scoped allocation costs; no IO, native device time, complete copies, RSS or UMA claim",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {"commit": report["commit"], "scopes": len(rows), "output": str(output)}
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
