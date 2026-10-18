"""Separate current-geometry legacy/stream verifier cost; not an engine benchmark."""

import argparse
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from cloud_experiment import view, view_bounds
from foliage_selection import directions
from leaf_patch_catalog import LeafPatchCatalog
from patch_coverage import coverage
from spatial_group_source import load_source


def measure(inputs, output, method):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private cost output must be under /tmp")
    source = load_source(inputs)
    exact = source.geometry(17)
    catalog = LeafPatchCatalog(source, 4)
    charts = catalog.charts(catalog.group(17))
    points = np.vstack((exact["vertices"], *[c.mesh["vertices"] for c in charts]))
    direction = directions(24)[12]
    bounds = view_bounds(points, direction)
    call = (
        (lambda: view(exact, charts, direction, bounds, 16, fields=True))
        if method == "legacy"
        else (lambda: coverage(exact, charts, direction, bounds, 16))
    )
    times = []
    for _ in range(5):
        before = time.perf_counter()
        metrics, reference, reduced = call()
        times.append(time.perf_counter() - before)
    tracemalloc.start()
    before = tracemalloc.take_snapshot()
    measured = call()
    current, peak = tracemalloc.get_traced_memory()
    after = tracemalloc.take_snapshot()
    tracemalloc.stop()
    np.testing.assert_array_equal(measured[1], reference)
    np.testing.assert_array_equal(measured[2], reduced)
    live = [x for x in after.compare_to(before, "traceback") if x.size_diff > 0]
    report = {
        "method": method,
        "times_s": times,
        "median_s": float(np.median(times)),
        "traced_live_bytes": current,
        "traced_peak_bytes": peak,
        "positive_live_allocation_count": sum(x.count_diff for x in live),
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "chart_count": len(charts),
        "raster_size": [128, 128],
        "chart_pixel_slots": len(charts) * 128 * 128,
        "sorted_chart_pixel_slots": len(charts) * 128 * 128
        if method == "legacy"
        else 0,
        "coverage_metrics": {
            k: v for k, v in metrics.items() if k != "light_response_relative_errors"
        },
        "scope": "Same current charts and unlit output. Legacy additionally shades/sorts; coverage-only stream avoids that unnecessary verifier work. Timing without tracing; memory in separate traced call. Live counts/peak are not cumulative allocation/traffic or GPU/native cost.",
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    np.savez_compressed(
        output.with_suffix(".npz"), reference=reference, reduced=reduced
    )
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for k in ("inputs", "output"):
        p.add_argument("--" + k, type=Path, required=True)
    p.add_argument("--method", choices=("legacy", "stream"), required=True)
    a = p.parse_args()
    measure(a.inputs, a.output, a.method)
