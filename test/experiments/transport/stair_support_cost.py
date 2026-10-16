"""Time complete bounded station plans against exactly the verified saved products."""

import argparse
import contextlib
import gc
import json
import resource
import statistics
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from rail_instance_cost import profile_note
from stair_support_plan import inputs, plan
from stair_terrain_experiment import frozen
from wien_support_experiment import packed


def measure(args):
    commit = frozen()
    if args.output.exists() or (args.profile and args.profile.exists()):
        raise ValueError("fresh explicit support cost/profile output required")
    report = json.loads((args.products / "comparison.json").read_text())
    replay = json.loads((args.products / "verification.json").read_text())
    if not replay["valid_local_candidate"] or replay[
        "products_report_sha256"
    ] != digest(args.products / "comparison.json"):
        raise ValueError("verified saved support products required")
    path = args.products / (args.mode + ".npz")
    if digest(path) != report["methods"][args.mode]["mesh_sha256"]:
        raise ValueError("measured support export changed")
    rows, road, lower, _context, _artifacts = inputs(
        Path(report["baseline"]), Path(report["fixture_path"])
    )
    expected = dict(np.load(path))
    times, matches, notes = [], [], []
    for run in range(2 if args.profile else 8):
        gc.collect()
        tracker = contextlib.nullcontext()
        if args.profile and run:
            import memray

            tracker = memray.Tracker(
                str(args.profile), trace_python_allocators=False, native_traces=False
            )
        began = time.perf_counter()
        with tracker:
            products, note = plan(
                rows, road, lower, method=args.mode, preparation=args.preparation
            )
        elapsed = (time.perf_counter() - began) * 1000
        actual = packed(
            [p.body for p in products],
            [(p.lower_contact, p.upper_contact) for p in products],
        )
        same = set(actual) == set(expected) and all(
            np.array_equal(actual[key], expected[key]) for key in actual
        )
        if run:
            times.append(elapsed)
            matches.append(same)
            notes.append(note)
        del products, actual
    result = {
        "commit": commit,
        "products_report_sha256": digest(args.products / "comparison.json"),
        "mode": args.mode,
        "preparation_work": notes[-1]["preparation_work"],
        "profiled": args.profile is not None,
        "phase_ms": times,
        "median_ms": statistics.median(times),
        "all_arrays_exact": all(matches),
        "exact_per_run": matches,
        "accepted_per_run": [n["accepted"] for n in notes],
        "work": notes[-1]["work"],
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "incomplete_runs": [
            n for n, same in zip(notes, matches, strict=True) if not same
        ],
        "scope": "Same source products and cooperative 8s/128-station/640-trial bounds. Preparation, construction and actual Double/Float32 admission timed; input decode, packed-array comparison and export outside. One warm run excluded; no global-optimum/device/frame/speed-factor claim.",
    }
    if args.profile:
        allocations = profile_note(str(args.profile))
        allocations.update(
            trace_python_allocators=False,
            native_traces=False,
            scope="One complete station plan including preparation, construction and physical checks after imports/source decode. Native allocations observed; Python allocator events/internal total copies unobserved. Requested bytes are not live or unique memory.",
        )
        result["allocations"] = allocations
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    if not all(matches):
        raise ValueError(
            "timed partial output differs from the verified full candidate"
        )
    print(
        json.dumps(
            {key: result[key] for key in ("mode", "median_ms", "all_arrays_exact")}
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--mode", choices=("fixed", "shifted"), required=True)
    parser.add_argument(
        "--preparation", choices=("shared", "per_source"), default="shared"
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    measure(parser.parse_args())
