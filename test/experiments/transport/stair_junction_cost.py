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


def saved_model(args):
    report = json.loads((args.products / "comparison.json").read_text())
    verified = json.loads((args.products / "verification.json").read_text())
    extra = {}
    if args.network is not None:
        from stair_network_geometry import interactions
        from stair_network_inputs import PROFILES, load

        if (
            report["fixture_sha256"] != digest(args.network)
            or report["profile_recipe_sha256"] != digest(PROFILES)
            or not verified["valid_local_bodies"]
            or digest(args.input) != report["native_case_sha256"]
        ):
            raise ValueError("actual verified three-stair inputs required")
        _case, rows, nodes, stairs, _context, _recipe = load(
            args.network, native_path=args.input
        )
        plan = layout(rows, "width", junctions=nodes, stairs=stairs)
        model = report["after"]
        input_file = args.products / "after/height-input.npz"
        expected = model["solve"]["objective"]
        pairs, _note = interactions(rows, nodes)
        extra = {"limits": [r["gradient"] for r in rows], "crossings": pairs}
    elif args.context is not None:
        from stair_context_experiment import crossings
        from stair_context_inputs import PROFILES, load

        if (
            report["fixture_sha256"] != digest(args.context)
            or report["profile_recipe_sha256"] != digest(PROFILES)
            or not verified["valid_local_bodies"]
            or digest(args.input) != report["native_case_sha256"]
        ):
            raise ValueError("actual verified contextual inputs required")
        _case, rows, junctions, _fixture = load(args.context, args.input)
        plan = layout(rows, "width", junctions=junctions)
        model = report["after"]
        input_file = args.products / "after/height-input.npz"
        expected = model["solve"]["objective"]
        extra = {"limits": [r["gradient"] for r in rows], "crossings": crossings(rows)}
    else:
        if not verified["valid"] or digest(args.input) != report["input_sha256"]:
            raise ValueError("actual verified four-way inputs required")
        _case, rows = inputs(args.input)
        plan = layout(rows, "width")
        model = report
        input_file = args.products / "height-input.npz"
        expected = report["comparisons"]["width_core"]["objective"]
    if report["dirty"] or digest(input_file) != model["height_input_sha256"]:
        raise ValueError("dirty or unbound measured source field")
    with np.load(input_file) as saved:
        if not np.array_equal(saved["xy"], plan["xy"]) or any(
            not np.array_equal(saved["faces" + str(i)], plan["parts"][i])
            for i in range(len(rows))
        ):
            raise ValueError("cost worker changed the saved actual finite model")
    return plan, expected, extra, input_file


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
    plan, expected, extra, input_file = saved_model(args)
    for method in ("highs-ds", "highs-ipm"):
        h, note = solve(plan, solver=method)
        if h is None or abs(note["objective"] - expected) > 1e-6:
            raise ValueError("independent finite-model objective differs")
        _body, data, _geometry = build(plan, h)
        audit(data, plan["domains"], **extra)
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    timings = []
    for _ in range(1 if args.profile else args.repetitions):
        del h
        gc.collect()
        began = time.perf_counter()
        with tracker:
            h, note = solve(plan, solver=args.solver)
        timings.append((time.perf_counter() - began) * 1000)
        if h is None or abs(note["objective"] - expected) > 1e-6:
            raise ValueError("measured finite solve lost its quality/objective")
        _body, data, _geometry = build(plan, h)
        audit(data, plan["domains"], **extra)
    row = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.products / "comparison.json"),
        "input_sha256": digest(input_file),
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
    parser.add_argument("--context", type=Path)
    parser.add_argument("--network", type=Path)
    parser.add_argument("--repetitions", type=int, choices=range(1, 22), default=11)
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    measure(parser.parse_args())
