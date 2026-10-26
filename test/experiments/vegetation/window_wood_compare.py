"""Full Blender-buffer equivalence and scoped old/new wood assembly costs."""

import argparse
import gc
import hashlib
import importlib.util
import json
import subprocess
import tempfile
import time
import tracemalloc
from pathlib import Path

import numpy as np
from window_portrait_source import source, wood

REFERENCE = "054e203c02e86676364aeb8b90e24a7150108926"


def measured(function, graph, axes):
    times = []
    for _ in range(3):
        began = time.perf_counter()
        data = function(graph, axes)
        times.append(time.perf_counter() - began)
        del data
    gc.collect()
    tracemalloc.start()
    data = function(graph, axes)
    live, peak = tracemalloc.get_traced_memory()
    snapshot = tracemalloc.take_snapshot()
    blocks = sum(s.count for s in snapshot.statistics("traceback"))
    tracemalloc.stop()
    return data, {
        "normal_3_run_median_s": float(np.median(times)),
        "one_call_traced_live_bytes": live,
        "one_call_traced_peak_bytes": peak,
        "one_call_traced_live_blocks": blocks,
        "output_array_bytes": sum(a.nbytes for a in data.values()),
    }


def compare(whole, execution, catalog, output):
    graph, axes, _, _, evidence = source(whole, execution, catalog)
    text = subprocess.check_output(
        [
            "git",
            "show",
            REFERENCE + ":test/experiments/vegetation/window_portrait_source.py",
        ],
        text=True,
    )
    with tempfile.TemporaryDirectory(
        prefix="outshine-vegetation-wood-reference-", dir="/tmp"
    ) as temp:
        path = Path(temp) / "original_wood.py"
        path.write_text(text)
        spec = importlib.util.spec_from_file_location("original_window_wood", path)
        previous = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(previous)
    before, old_cost = measured(previous.wood, graph, axes)
    after, new_cost = measured(wood, graph, axes)
    fields = {}
    for name in ("vertices", "faces", "normals"):
        dtype = np.int32 if name == "faces" else np.float32
        expected = before[name].astype(dtype)
        np.testing.assert_array_equal(after[name], expected)
        fields[name] = {
            "shape": list(after[name].shape),
            "dtype": str(after[name].dtype),
            "same_blender_input": True,
            "sha256": hashlib.sha256(after[name].tobytes()).hexdigest(),
            "max_source_rounding": float(np.max(abs(before[name] - after[name]))),
        }
    result = {
        "identity": evidence["identity"],
        "reference_commit": REFERENCE,
        "reference_source_sha256": hashlib.sha256(text.encode()).hexdigest(),
        "fields": fields,
        "old": old_cost,
        "new": new_cost,
        "scope": "Identical final float32/int32 Blender geometry/normal attributes for the whole cached tree. Plans/source arrays remain float64. Traces are separate calls, cover traced Python/NumPy allocations rather than all native allocations, and live blocks are not total allocation events. Timing is a small same-process local comparison, not GPU/frame/device performance. The preceding full llvmpipe render was terminal before these runs.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("whole", "execution", "catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    compare(args.whole, args.execution, args.catalog, args.output)
