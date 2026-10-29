"""Equal-result endpoint/corner projection costs with already held metadata."""

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
from capture_source_support import capture_support
from capture_support_reference import corner_support, metadata_inputs

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render/building-field-composition"


def equal(a, b):
    if a.keys() != b.keys():
        raise ValueError("Projection result contracts differ")
    for key in a:
        np.testing.assert_array_equal(a[key], b[key])


def measured(methods, output, prefix):
    names = tuple(methods)
    expected = methods[names[0]]()
    for call in methods.values():
        equal(expected, call())
    report = {n: {"rounds": []} for n in names}
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
            equal(expected, value)
            del value
    for name in names:
        gc.collect()
        value, memory = allocations(output / f"{prefix}-{name}.bin", methods[name])
        equal(expected, value)
        report[name].update(memory)
        for clock in ("cpu", "wall"):
            report[name][clock + "_median_ms"] = 1000 * float(
                np.median([r[clock + "_s"] for r in report[name]["rounds"]])
            )
    return report


def run(args):
    producer = [
        Path(__file__),
        Path(__file__).with_name("capture_source_support.py"),
        Path(__file__).with_name("capture_support_reference.py"),
        Path(__file__).with_name("field_view_projection.py"),
        ROOT / "test/experiments/buildings/allocation_measure.py",
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, producer)], text=True
    ):
        raise ValueError("Freeze the measured code before recording costs")
    output = args.output.resolve()
    if (
        output.is_relative_to(ROOT)
        or not output.is_relative_to(Path(tempfile.gettempdir()).resolve())
        or output == Path(tempfile.gettempdir()).resolve()
    ):
        raise ValueError("Use a fresh explicit system-temp output")
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Measure at normal process priority")
    output.mkdir(parents=True, exist_ok=False)
    manifest, arrays = metadata_inputs(args.data)
    lower, upper = arrays["source_least_m"], arrays["source_most_m"]
    records = []
    load_before = os.getloadavg()
    for name in ("foreground", "separated-background", "interleaved-background"):
        args_tuple = (lower, upper, manifest["capture_camera"])
        options = {"translation": manifest["controlled_translations_m"][name][0]}
        methods = {
            "endpoints": partial(capture_support, *args_tuple, **options),
            "corners": partial(corner_support, *args_tuple, **options),
        }
        costs = measured(methods, output, name)
        result = methods["endpoints"]()
        records.append(
            {
                "placement": name,
                "sources": len(lower),
                "retained": int((~result["inside"]).sum()),
                "costs": costs,
                "owned_result_array_bytes": sum(a.nbytes for a in result.values()),
            }
        )
    n = len(lower)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "input_npz_sha256": manifest["npz_sha256"],
        "metadata_array_bytes": sum(a.nbytes for a in arrays.values()),
        "load_before": load_before,
        "load_after": os.getloadavg(),
        "rows": records,
        "work": {
            "endpoint_interval_coordinate_products": 18 * n,
            "corner_interval_coordinate_products": 144 * n,
            "expanded_corner_points": 8 * n,
            "endpoint_expanded_corner_points": 0,
            "known_float32_roundtrip_copy_bytes_each": 2 * 3 * n * (4 + 8),
            "known_corner_gather_copy_bytes": 8 * 3 * n * 8,
            "total_copy_bytes": None,
        },
        "scope": "Nine rotating normal-priority CPU/wall rounds, comparison outside timing and separate Memray/native allocation traces. Identical result masks/intervals/limits. Inputs and held opponent precede scoped peaks. Metadata fixture checksum/decode, source geometry, images and GPU work excluded; no cold-IO, native frame/device or UMA claim. Fixed Double camera model, not a GPU transform/raster certificate.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            [
                {
                    **{k: v for k, v in r.items() if k != "costs"},
                    "costs": {
                        k: {a: b for a, b in v.items() if a != "rounds"}
                        for k, v in r["costs"].items()
                    },
                }
                for r in records
            ]
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
