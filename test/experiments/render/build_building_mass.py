"""Build one bounded geometric parent candidate from BSC3 forms, never native mesh."""

import argparse
import gc
import json
import os
import subprocess
import time
import tracemalloc
from pathlib import Path

import numpy as np
from building_mass_product import build
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes


def cook(path, cells):
    with open_parent(path) as parent:
        allowed = {parent.metadata["index"], *(parent.forms[c] for c in cells)}
        member = parent.member

        def source_only(name):
            if name not in allowed:
                raise ValueError(
                    "mass generator must not read native view/triangle input"
                )
            return member(name)

        parent.member = source_only
        return build(parent, cells)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    load = os.getloadavg()
    wall, cpu = time.perf_counter(), time.process_time()
    arrays, report = cook(args.input, set(args.cells))
    report["cost"] = {
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "wall_ms": (time.perf_counter() - wall) * 1000,
        "process_peak_after_build_bytes": peak_rss_bytes(),
        "load_before": load,
    }
    if args.trace:
        gc.collect()
        tracemalloc.start()
        replay, _ = cook(args.input, set(args.cells))
        _, peak = tracemalloc.get_traced_memory()
        live = sum(s.count for s in tracemalloc.take_snapshot().statistics("filename"))
        tracemalloc.stop()
        if not all(arrays[k].tobytes() == replay[k].tobytes() for k in arrays):
            raise ValueError("traced replay changed source-first parent output")
        report["traced_replay"] = {
            "same_output_words": True,
            "python_traced_peak_bytes": peak,
            "python_traced_live_blocks": live,
            "scope": "Whole second source-read/build replay; not cumulative allocations, GEOS heap or physical copy traffic. Both alternatives remain in process memory.",
        }
    report["producer"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip()
    np.savez(args.output / "product.npz", **arrays)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "groups": len(report["groups"]),
                "sources": len(arrays["group_sources"]),
                "needs": len(arrays["needs_sources"]),
                "triangles": len(arrays["indices"]),
                "bytes": report["all_array_bytes"],
                "cost": report["cost"],
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    main()
