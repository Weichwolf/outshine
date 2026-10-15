"""Isolated equal-output family-area costs on 1024 deterministic actual road facets."""

import argparse
import contextlib
import gc
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from rail_instance_cost import profile_note
from transport_surface_areas import family_areas
from transport_surface_inputs import inputs


def measure(args):
    if args.output.exists() or args.profile and args.profile.exists():
        raise ValueError("fresh family-area cost output required")
    report = json.loads((args.products / "comparison.json").read_text())
    ground, road, selected, _records, _arrays, binding = inputs(
        args.network, args.baseline
    )
    if report["dirty"] or binding != report["bindings"]:
        raise ValueError("clean bound actual source/geometry required")
    source = args.products / ("areas-" + args.method + ".npz")
    if digest(source) != report["methods"][args.method]["sha256"]:
        raise ValueError("actual comparison geometry changed")
    wanted = np.linspace(0, len(selected) - 1, min(1024, len(selected))).astype(int)
    with np.load(source) as d:
        expected = d["areas_xy_m2"][wanted]
    values = json.loads((args.products / "families.json").read_text())
    if digest(args.products / "families.json") != report["families_sha256"]:
        raise ValueError("actual family domains changed")
    domains = [shapely.from_wkb(bytes.fromhex(s)) for s in values["domains_wkb_hex"]]
    triangles = ground.vertices[ground.triangles[road][wanted], :2]
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    timings = []
    output = None
    for _ in range(1 if args.profile else 7):
        del output
        gc.collect()
        began = time.perf_counter()
        with tracker:
            output, reasons, work = family_areas(
                triangles, domains, method=args.method, seconds=60
            )
        timings.append((time.perf_counter() - began) * 1000)
    error = float(np.max(abs(output - expected)))
    if error > 1e-7 or np.any(reasons):
        raise ValueError("measured phase differs from saved actual family-area rows")
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_report_sha256": digest(args.products / "comparison.json"),
        "method": args.method,
        "profiled": args.profile is not None,
        "phase_ms": timings,
        "median_ms": float(np.median(timings)),
        "facets": len(triangles),
        "work": work,
        "maximum_area_error_m2": error,
        "owned_area_array_bytes": output.nbytes,
        "owned_reason_array_bytes": reasons.nbytes,
        "known_sample_xyz_copy_bytes": triangles.nbytes,
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Only family decomposition/index and actual 1024 facet intersections after identical decode and source family construction. Equal saved actual per-face areas within1e-7 m2; seven normal calls or one instrumented call. Full 95456-face build measured separately. Known arrays are not all internal GEOS/copy traffic; no native/device/renderer speed claim.",
    }
    if args.profile:
        result["allocations"] = profile_note(str(args.profile))
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["network", "baseline", "products", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--method", choices=["whole", "cells"], required=True)
    parser.add_argument("--profile", type=Path)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    measure(args)
