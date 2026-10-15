"""Isolated dilation costs on the same actual accepted curb footprints."""

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
from curb_verify import actual
from rail_instance_cost import profile_note
from retained_surface import surface_domain
from sidewalk_inputs import CURB_BODY
from sidewalk_plan import dilation


def measure(args):
    if args.output.exists() or args.profile and args.profile.exists():
        raise ValueError("fresh sidewalk cost outputs required")
    path = args.curbs / "curbs.npz"
    if digest(path) != CURB_BODY:
        raise ValueError("actual curb input changed")
    products, _ = actual(path)
    shapes = [surface_domain(p.lower_contact) for p in products]
    expected = dilation(shapes, "combined")
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    timings = []
    value = None
    for _ in range(1 if args.profile else 7):
        del value
        gc.collect()
        began = time.perf_counter()
        with tracker:
            value = dilation(shapes, args.method)
        timings.append((time.perf_counter() - began) * 1000)
    difference = expected.symmetric_difference(value).area
    if difference > 1e-8:
        raise ValueError("measured sidewalk dilation differs physically")
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "actual_curbs_sha256": CURB_BODY,
        "method": args.method,
        "profiled": args.profile is not None,
        "curb_footprints": len(shapes),
        "input_coordinates": int(np.sum(shapely.get_num_coordinates(shapes))),
        "output_coordinates": int(shapely.get_num_coordinates(value)),
        "output_wkb_bytes": len(shapely.to_wkb(value)),
        "phase_ms": timings,
        "median_ms": float(np.median(timings)),
        "symmetric_difference_m2": difference,
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Only actual 256-curb dilation/union after decode and footprint preparation. No clipping, volume/audit/packing/render work, no complete internal copy count or frame/device claim. Expected mask retained equally by both workers; profiled time separate.",
    }
    if args.profile:
        result["allocations"] = profile_note(str(args.profile))
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["curbs", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--method", choices=["combined", "individual"], required=True)
    parser.add_argument("--profile", type=Path)
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    measure(parser.parse_args())
