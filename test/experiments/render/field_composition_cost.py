"""Costs of equal coupled-sample kernels and the existing joint prefix path."""

import argparse
import gc
import hashlib
import json
import os
import platform
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
from allocation_measure import allocations
from field_composition import compose_points
from field_view_integrals import box_integrals, summed_field


def arithmetic(front, back):
    result = (1 - front[:, :, 3:4]) * back
    result += front
    return result


def measure(methods, output):
    names = tuple(methods)
    expected = {name: call() for name, call in methods.items()}
    np.testing.assert_array_equal(
        expected["arithmetic-kernel"], expected["select-kernel"]
    )
    np.testing.assert_array_equal(
        expected["arithmetic-kernel"], expected["checked-compose"]
    )
    report = {name: {"rounds": []} for name in names}
    for iteration in range(9):
        order = names[iteration % len(names) :] + names[: iteration % len(names)]
        for name in order:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            value = methods[name]()
            report[name]["rounds"].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            np.testing.assert_array_equal(value, expected[name])
            del value
    for name, call in methods.items():
        gc.collect()
        value, memory = allocations(output / (name + ".bin"), call)
        np.testing.assert_array_equal(value, expected[name])
        report[name].update(memory)
        for clock in ("cpu", "wall"):
            report[name][clock + "_median_ms"] = 1000 * float(
                np.median([r[clock + "_s"] for r in report[name]["rounds"]])
            )
    return report


def run(args):
    root = Path(__file__).resolve().parents[3]
    producer_paths = [
        Path(__file__),
        Path(__file__).with_name("field_composition.py"),
        Path(__file__).with_name("field_view_projection.py"),
        Path(__file__).with_name("field_view_integrals.py"),
        root / "test/experiments/buildings/allocation_measure.py",
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, producer_paths)], text=True
    ):
        raise ValueError("Freeze the measured producer before recording costs")
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Costs require normal process priority")
    if args.output.resolve().is_relative_to(
        root
    ) or not args.output.resolve().is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        raise ValueError("Use an explicit system-temp output outside the checkout")
    args.output.mkdir(parents=True, exist_ok=False)
    record = json.loads((args.control / "report.json").read_text())
    row = next(r for r in record["rows"] if r["name"] == "ordered-complete-sources")
    h, w = row["pixel_patch_shape"]
    samples = row["samples_axis"]
    with np.load(args.control / (row["name"] + ".npz")) as data:
        front, back = [
            data[k].astype(np.float32).reshape(h * samples, w * samples, 4)
            for k in ("point_front", "point_back")
        ]
    combined = compose_points(front, back)
    y, x = np.mgrid[:h, :w]
    lower = np.column_stack((x.ravel(), y.ravel())).astype(float) * samples
    upper = lower + samples
    held = summed_field(combined)

    def prefix():
        return summed_field(compose_points(front, back)).table

    methods = {
        "arithmetic-kernel": lambda: arithmetic(front, back),
        "select-kernel": lambda: np.where(front[:, :, 3:4] == 1, front, back),
        "checked-compose": lambda: compose_points(front, back),
        "compose-and-prefix": prefix,
        "held-prefix-lookups": lambda: box_integrals(held, lower, upper) / samples**2,
    }
    load_before = os.getloadavg()
    measurements = measure(methods, args.output)
    direct = combined.reshape(h, samples, w, samples, 4).mean((1, 3)).reshape(-1, 4)
    approximate = methods["held-prefix-lookups"]()
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "input_report_sha256": hashlib.sha256(
            (args.control / "report.json").read_bytes()
        ).hexdigest(),
        "load_before": load_before,
        "load_after": os.getloadavg(),
        "measurements": measurements,
        "sample_pairs": h * w * samples**2,
        "output_pixels": h * w,
        "held_input_bytes": front.nbytes + back.nbytes,
        "combined_output_bytes": combined.nbytes,
        "prefix_bytes": held.table.nbytes,
        "float32_prefix_max_abs_rgba_error": float(abs(direct - approximate).max()),
        "work": {
            "arithmetic_scalar_operations": 9 * h * w * samples**2,
            "select_point_comparisons": h * w * samples**2,
            "select_rgba_elements": 4 * h * w * samples**2,
            "prefix_logical_rgba_fetches": 16 * h * w,
            "output_bytes_written_each_composition": combined.nbytes,
            "known_owned_prefix_copy_bytes": held.table.nbytes,
            "total_copy_bytes": None,
        },
        "scope": "Nine rotating normal-priority CPU/wall rounds, comparisons outside timing; separate Memray native-allocation traces. Kernels exclude validation; checked compose includes it. Compose-and-prefix includes validation, combination and held-table construction, not source rays/shading or background acquisition. Prefix lookups use an already held joint field. Scoped peaks exclude held inputs/opponents/tables; not RSS/UMA, native device/frame time or general error admission. A changed bound background invalidates the joint product.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                name: {k: v for k, v in r.items() if k != "rounds"}
                for name, r in measurements.items()
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--control", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
