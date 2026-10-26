"""Equivalent scalar/batched rigid transforms on actual saved rail vertices only."""

import argparse
import contextlib
import gc
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from rail_instance_cost import profile_note
from tilted_rail_geometry import transform_points


def measure(args):
    if args.output.exists() or args.profile and args.profile.exists():
        raise ValueError("fresh transform cost output required")
    report = json.loads((args.products / "comparison.json").read_text())
    if report["dirty"]:
        raise ValueError("clean saved rail products required")
    sources = []
    for row in report["pieces"]:
        if not row["valid"]:
            continue
        path = args.products / "pieces" / f"piece-{row['product']}.npz"
        if digest(path) != row["mesh_sha256"]:
            raise ValueError("actual transform source changed")
        with np.load(path) as d:
            sources.append(
                tuple(
                    d[k]
                    for k in [
                        "local_rail_vertices",
                        "plane_origin",
                        "plane_frame",
                        "vertices",
                    ]
                )
            )
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    timings, output = [], None
    for _ in range(1 if args.profile else 7):
        del output
        gc.collect()
        began = time.perf_counter()
        with tracker:
            output = [
                transform_points(v, o, f, method=args.method)
                for v, o, f, _expected in sources
            ]
        timings.append((time.perf_counter() - began) * 1000)
    error = max(
        float(np.max(abs(actual - source[3]), initial=0))
        for actual, source in zip(output, sources, strict=True)
    )
    if error > 1e-9:
        raise ValueError("equivalent transform changed physical rail coordinates")
    points = sum(len(s[0]) for s in sources)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_commit": report["commit"],
        "report_sha256": digest(args.products / "comparison.json"),
        "method": args.method,
        "profiled": args.profile is not None,
        "phase_ms": timings,
        "median_ms": float(np.median(timings)),
        "pieces": len(sources),
        "vertices": points,
        "logical_scalar_operations": 18 * points,
        "python_point_dispatches": points if args.method == "scalar" else len(sources),
        "owned_output_bytes": sum(a.nbytes for a in output),
        "maximum_position_error_m": error,
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Only rigid transforms of original saved local rail vertices. Source decode, frame/profile construction, sleepers, footprint/admission, Mesh copies, geometry replay, export and renderer excluded. Seven normal repetitions; one instrumented repetition. Output payload is a known ownership amount, not every internal copy. Equal scalar arithmetic; Python dispatch/allocation work differs. No native/device speed claim.",
    }
    if args.profile:
        result["allocations"] = profile_note(str(args.profile))
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--method", choices=("batch", "scalar"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (30, 30))
    measure(args)
