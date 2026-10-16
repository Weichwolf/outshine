"""Compare both solvers on identical saved multi-cell profiles, after warm imports."""

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
from cycle_bridge_profile import solve
from rail_instance_cost import profile_note


def measure(args):
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
        raise ValueError("layered profile cost requires frozen clean research sources")
    if args.output.exists() or (args.profile is not None and args.profile.exists()):
        raise ValueError("fresh layered profile cost outputs required")
    report = json.loads((args.products / "comparison.json").read_text())
    verified = json.loads((args.products / "verification.json").read_text())
    if (
        report["dirty"]
        or not verified["valid"]
        or digest(args.products / "profile-input.npz") != report["profile_input_sha256"]
    ):
        raise ValueError("bound clean and replayed actual layered profiles required")
    with np.load(args.products / "profile-input.npz") as data:
        inputs = {
            n: [data[n + "_" + k] for k in ("stations", "lower", "upper", "rate")]
            for n in report["admitted_profiles"]
        }
    expected = {}
    for name, arrays in inputs.items():
        expected[name], _note = solve(*arrays, method="chain")
        warm, _note = solve(*arrays, method="lp")
        np.testing.assert_allclose(warm, expected[name], rtol=0, atol=1e-8)
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=True
        )
    timings, result, work = [], {}, {}
    for _ in range(1 if args.profile else 11):
        result.clear()
        gc.collect()
        began = time.perf_counter()
        with tracker:
            for name, arrays in inputs.items():
                result[name], work[name] = solve(*arrays, method=args.method)
        timings.append((time.perf_counter() - began) * 1000)
        for name in inputs:
            np.testing.assert_allclose(result[name], expected[name], rtol=0, atol=1e-8)
    row = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.products / "comparison.json"),
        "input_sha256": digest(args.products / "profile-input.npz"),
        "method": args.method,
        "profiled": args.profile is not None,
        "phase_ms": timings,
        "median_ms": float(np.median(timings)),
        "profile_stations": {n: len(a[0]) for n, a in inputs.items()},
        "input_array_bytes": sum(
            a.nbytes for arrays in inputs.values() for a in arrays
        ),
        "output_array_bytes": sum(a.nbytes for a in result.values()),
        "work": work,
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Two fixed-chain profile solves/audits after input decode and warm imports/solves. Eleven normal repetitions or one separate allocation trace. No meshing, whole-generator/native/device timing or internal copy-total claim. Chain explicitly copies one lower-bound vector per profile; LP reports retained sparse matrix bytes, not internal allocations.",
    }
    if args.profile:
        row["allocations"] = profile_note(str(args.profile))
        row["allocations"]["scope"] = row["scope"]
    args.output.write_text(json.dumps(row, indent=2) + "\n")
    print(json.dumps(row), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--method", choices=("chain", "lp"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (60, 60))
    measure(parser.parse_args())
