"""Optional minimax tie selection: same feasibility, reduced path creases at extra cost."""

import argparse
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
from compare_stair_envelopes import run
from mvt_native_body_display import display
from stair_profile_quality import path_creases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--selection", choices=("minimax", "minimax_then_variation"), default="minimax"
    )
    parser.add_argument(
        "--solver", choices=("highs-ds", "highs-ipm"), default="highs-ds"
    )
    parser.add_argument("--trace-memory", action="store_true")
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    if args.trace_memory:
        tracemalloc.start()
    began = time.perf_counter()
    plan, data, products, report = run(
        "envelope_indexed_band_remesh",
        False,
        selection=args.selection,
        solver=args.solver,
    )
    elapsed = time.perf_counter() - began
    if args.trace_memory:
        _current, report["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        report["python_traced_live_blocks"] = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    report.update(
        selection=args.selection,
        solver=args.solver,
        pipeline_s=elapsed,
        instrumented=args.trace_memory,
        path_creases=path_creases(plan, data["reference_vertices"][:, 2]),
        scope="Local Python layout/fit/body/audits/shared products, before crease diagnostic, image and exports. Optional tracing covers this whole pipeline; RSS is whole-process peak. No native/device/frame claim.",
    )
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(args.output / "body.npz", **data)
    (args.output / "shared-plan.json").write_text(json.dumps(products, indent=2) + "\n")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.image:
        display(plan, data, args.output)
    print(json.dumps({"selection": args.selection, "pipeline_s": elapsed}))


if __name__ == "__main__":
    main()
