"""Scoped buffer/trace costs for actual cached window geometry; not frame timing."""

import argparse
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from mature_tree_portrait import SCENE
from window_portrait_geometry import planes, select_views
from window_portrait_source import source, wood


def measure(whole, execution, catalog, output):
    graph, axes, windows, banks, evidence = source(whole, execution, catalog)
    started = time.perf_counter()
    selected = select_views(windows, 64, eye=SCENE["camera"])
    select_seconds = time.perf_counter() - started
    parts, plane_sizes = [], []
    for family, bank in enumerate(banks):
        indices = np.flatnonzero(windows["families"] == family)
        times = []
        for _ in range(7):
            began = time.perf_counter()
            data = planes(windows, bank, selected, indices, family)
            times.append(time.perf_counter() - began)
        del data
        tracemalloc.start()
        before = tracemalloc.take_snapshot()
        data = planes(windows, bank, selected, indices, family)
        current, peak = tracemalloc.get_traced_memory()
        after = tracemalloc.take_snapshot()
        live_blocks = sum(s.count_diff for s in after.compare_to(before, "traceback"))
        tracemalloc.stop()
        parts.append(
            {
                "family": family,
                "windows": len(indices),
                "normal_7_run_median_s": float(np.median(times)),
                "one_call_trace_live_bytes": current,
                "one_call_trace_peak_bytes": peak,
                "one_call_trace_live_block_delta": live_blocks,
            }
        )
        plane_sizes.append((len(data["vertices"]), len(data["faces"])))
    started = time.perf_counter()
    woody = wood(graph, axes)
    wood_seconds = time.perf_counter() - started
    vertices, faces = len(woody["vertices"]), len(woody["faces"])
    result = {
        "identity": evidence["identity"],
        "window_camera_select_s": select_seconds,
        "directions_scored_per_role": len(windows["origins"]) * 64,
        "window_geometry_per_family": parts,
        "wood_assembly_s": wood_seconds,
        "wood_vertices": vertices,
        "wood_triangles": faces,
        "wood_output_array_bytes": sum(a.nbytes for a in woody.values()),
        "wood_explicit_blender_buffer_write_bytes": vertices * 12 + faces * 56,
        "window_explicit_blender_buffer_write_bytes_all_3_roles": 3
        * sum(v * 12 + f * 48 for v, f in plane_sizes),
        "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "scope": "Normal seven-run median for one existing physical plane batch; trace is one separate call, live block deltas are not cumulative allocations. NumPy/CPython trace only, no total native/Blender/driver accounting. Explicit float32/int32 binding writes exclude decoder, intermediate arrays, internal Blender copies and GPU upload. Concurrent full llvmpipe render; not isolated timing, a speedup claim or a device/frame result.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("whole", "execution", "catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    measure(args.whole, args.execution, args.catalog, args.output)
