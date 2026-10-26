"""Same complete-profile guard: general union versus spatially selected union."""

import argparse
import json
import resource
import sys
import time
import tracemalloc
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from compare_stair_envelopes import run
from mvt_native_body_display import display
from mvt_t_profile_products import _supported_triangles, whole_section


def _union_triangles(line, triangles, tolerance):
    domain = shapely.union_all(triangles)
    if not domain.covers(line) and not (
        tolerance and domain.buffer(tolerance).covers(line)
    ):
        raise ValueError("whole profile has unsupported intervals")
    return triangles


def _capture():
    calls = []

    def capture(line, triangles, sampler, extra_xy):
        result = whole_section(line, triangles, sampler, extra_xy)
        calls.append((line, triangles, sampler, extra_xy, result))
        return result

    with patch("mvt_t_profile_products.whole_section", capture):
        plan, data, products, body_report = run("envelope_indexed_band_remesh", False)
    return calls, plan, data, products, body_report


def _check_gap(implementation):
    rectangles = shapely.polygons(
        [
            [[-2, -1], [-0.05, -1], [-0.05, 1], [-2, 1]],
            [[0.05, -1], [2, -1], [2, 1], [0.05, 1]],
        ]
    )
    line = shapely.LineString([[-1, 0], [1, 0]])
    try:
        implementation(line, rectangles, 0)
    except ValueError:
        return
    raise ValueError("candidate silently bridged the analytic ten-centimetre gap")


def compare(calls, method, repeats, trace):
    implementation = _supported_triangles if method == "index" else _union_triangles
    _check_gap(implementation)
    work = {"profile_queries": 0, "input_faces": 0, "union_operands": 0}

    def recorded(line, triangles, tolerance):
        selected = implementation(line, triangles, tolerance)
        work["profile_queries"] += 1
        work["input_faces"] += len(triangles)
        work["union_operands"] += len(selected)
        return selected

    with patch("mvt_t_profile_products._supported_triangles", recorded):
        for line, triangles, sampler, extra, expected in calls:
            np.testing.assert_array_equal(
                whole_section(line, triangles, sampler, extra), expected
            )
    if trace:
        tracemalloc.start()
    began = time.perf_counter()
    with patch("mvt_t_profile_products._supported_triangles", implementation):
        for _ in range(repeats):
            for line, triangles, sampler, extra, _expected in calls:
                whole_section(line, triangles, sampler, extra)
    elapsed = time.perf_counter() - began
    memory = {}
    if trace:
        _current, memory["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        memory["python_traced_live_blocks"] = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    return {
        "method": method,
        "instrumented": trace,
        "repeats": repeats,
        "profile_batch_s": elapsed / repeats,
        "work_per_batch": work,
        "selected_geometry_pointer_bytes_per_batch": work["union_operands"]
        * np.dtype(object).itemsize
        if method == "index"
        else 0,
        "selected_index_array_bytes_per_batch": work["union_operands"]
        * np.dtype(np.intp).itemsize
        if method == "index"
        else 0,
        "process_peak_rss_bytes": rss if sys.platform == "darwin" else rss * 1024,
        **memory,
        "scope": "Only repeated whole-profile calls on 11 captured real native queries, after the same layout/fit/body/audits/products. Candidate checks precede timing. Tracing covers this profile loop; GEOS/native allocations are incomplete. RSS covers the whole process. Pointer/index bytes are identified array copies, not a peak/copy total. No native/device/frame claim.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--method", choices=("index", "union"), default="index")
    parser.add_argument("--repeats", type=int, default=10)
    parser.add_argument("--trace-memory", action="store_true")
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 100:
        parser.error("bounded repeat count in [1,100] required")
    calls, plan, data, products, body_report = _capture()
    report = compare(calls, args.method, args.repeats, args.trace_memory)
    report["body_report"] = body_report
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(args.output / "body.npz", **data)
    (args.output / "shared-plan.json").write_text(json.dumps(products, indent=2) + "\n")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.image:
        display(plan, data, args.output)
    print(
        json.dumps(
            {"method": args.method, "profile_batch_s": report["profile_batch_s"]}
        )
    )


if __name__ == "__main__":
    main()
