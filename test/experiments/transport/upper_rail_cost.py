"""Isolated selection costs on identical actual deck/source inputs and quality rules."""

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
from rail_bed_patches import contact_patches, cut_to_patches
from rail_instance_cost import profile_note
from upper_rail_inputs import inputs


def measure(args):
    if args.output.exists() or args.profile and args.profile.exists():
        raise ValueError("fresh selection cost outputs required")
    deck, candidates, _raw, _chains, _work, binding = inputs(args.reference)
    expected = json.loads((args.products / "comparison.json").read_text())
    if (
        expected["dirty"]
        or expected["bindings"] != binding
        or expected["method"] != args.method
    ):
        raise ValueError("clean bound actual selection required")
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
            patches, preparation = contact_patches(deck, method=args.method)
            output, ledger, work = cut_to_patches(candidates, patches)
        timings.append((time.perf_counter() - began) * 1000)
    membership = [{"plane_face": p["plane_face"], "faces": p["faces"]} for p in patches]
    if ledger != expected["affine_cuts"] or membership != expected["patches"]:
        raise ValueError("instrumented selection changed actual candidate coverage")
    if len(output) != len(expected["pieces"]) or any(
        not np.array_equal(p["xy"], r["xy"]) or p["plane_face"] != r["plane_face"]
        for p, r in zip(output, expected["pieces"], strict=True)
    ):
        raise ValueError("instrumented selection changed actual station arrays")
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.products / "comparison.json"),
        "method": args.method,
        "profiled": args.profile is not None,
        "phase_ms": timings,
        "median_ms": float(np.median(timings)),
        "preparation": preparation,
        "cutting": work,
        "input_faces": len(deck.triangles),
        "input_axis_pieces": len(candidates),
        "output_axis_pieces": len(output),
        "output_axis_xy_array_bytes": sum(p["xy"].nbytes for p in output),
        "candidate_length_xy_m": sum(r["candidate_length_m"] for r in ledger),
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Only actual bed partition and source cutting after identical decode. Same whole-affine/contact inset rules; patch method intentionally admits more valid axes. No equal-output or whole-generator speed claim. Seven normal repetitions or one Memray call; instrumented times separate. XY arrays are explicit owned copies only; GEOS arrays/internal copies not completely counted. RSS includes decoded source/deck and imports; no renderer/native/device acceptance.",
    }
    if args.profile:
        result["allocations"] = profile_note(str(args.profile))
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["reference", "products", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--method", choices=["triangle", "patch"], required=True)
    parser.add_argument("--profile", type=Path)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (30, 30))
    measure(args)
