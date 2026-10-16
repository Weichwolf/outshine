"""Same finite model and whole-body acceptance; two independent HiGHS algorithms."""

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
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import INPUT, inputs, layout
from stair_junction_solve import solve


def measure(args):
    if args.output.exists() or (args.profile and args.profile.exists()):
        raise ValueError("fresh stair cost/profile outputs required")
    if subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--",
            "lib",
            str(Path(__file__).resolve().parent),
        ],
        text=True,
    ).strip():
        raise ValueError("frozen clean stair cost worker required")
    report = json.loads((args.products / "comparison.json").read_text())
    verified = json.loads((args.products / "verification.json").read_text())
    if (
        report["dirty"]
        or not verified["valid"]
        or digest(args.input) != report["input_sha256"]
        or digest(args.products / "height-input.npz") != report["height_input_sha256"]
    ):
        raise ValueError("actual verified four-way inputs required")
    _case, rows = inputs(args.input)
    plan = layout(rows, "width")
    with np.load(args.products / "height-input.npz") as saved:
        if not np.array_equal(saved["xy"], plan["xy"]) or any(
            not np.array_equal(saved["faces" + str(i)], plan["parts"][i])
            for i in range(4)
        ):
            raise ValueError("cost worker changed the saved actual finite model")
    expected = report["comparisons"]["width_core"]["objective"]
    for method in ("highs-ds", "highs-ipm"):
        h, note = solve(plan, solver=method)
        if h is None or abs(note["objective"] - expected) > 1e-6:
            raise ValueError("independent finite-model objective differs")
        _body, data, _geometry = build(plan, h)
        audit(data, plan["domains"])
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    timings = []
    for _ in range(1 if args.profile else 11):
        del h
        gc.collect()
        began = time.perf_counter()
        with tracker:
            h, note = solve(plan, solver=args.solver)
        timings.append((time.perf_counter() - began) * 1000)
        if h is None or abs(note["objective"] - expected) > 1e-6:
            raise ValueError("measured finite solve lost its quality/objective")
        _body, data, _geometry = build(plan, h)
        audit(data, plan["domains"])
    row = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.products / "comparison.json"),
        "input_sha256": digest(args.products / "height-input.npz"),
        "solver": args.solver,
        "phase_ms": timings,
        "median_ms": float(np.median(timings)),
        "profiled": args.profile is not None,
        "work": note,
        "output_array_bytes": h.nbytes,
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Finite condition preparation plus one LP after saved source decode/layout and warm solves. Whole-body audit outside timing validates every measured result. Input ndarray conversions explicitly counted; sparse retained bytes are not internal-copy totals. Normal repetitions and separate allocation traces; no native/frame/device or complete-generator factor.",
    }
    if args.profile:
        row["allocations"] = profile_note(str(args.profile))
        row["allocations"]["scope"] = row["scope"]
    args.output.write_text(json.dumps(row, indent=2) + "\n")
    print(json.dumps(row), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=INPUT)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--solver", choices=("highs-ds", "highs-ipm"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    measure(parser.parse_args())
