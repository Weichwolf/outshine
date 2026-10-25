"""Build reusable material data against an existing frozen source-first parent."""

import argparse
import gc
import hashlib
import json
import os
import subprocess
import time
import tracemalloc
from pathlib import Path

import numpy as np
from building_mass_appearance import build_appearance
from building_mass_materials import policy
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes


def cook(path, cells, geometry, at):
    with open_parent(path) as parent:
        allowed = {parent.metadata["index"], *(parent.forms[c] for c in cells)}
        member = parent.member

        def source_only(name):
            if name not in allowed:
                raise ValueError(
                    "material generator must read only selected source forms"
                )
            return member(name)

        parent.member = source_only
        try:
            arrays, report = build_appearance(parent, cells, geometry, at)
            report["source_archive_sha256"] = parent.archive_sha256
            return arrays, report
        finally:
            del parent.member


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--geometry", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    at = policy()
    with np.load(args.geometry, allow_pickle=False) as archive:
        geometry = dict(archive)
    load, wall, cpu = os.getloadavg(), time.perf_counter(), time.process_time()
    arrays, report = cook(args.input, set(args.cells), geometry, at)
    arrays["geometry_sha256"] = np.frombuffer(
        hashlib.sha256(args.geometry.read_bytes()).digest(), np.uint8
    ).copy()
    report["geometry_hash_bytes"] = arrays["geometry_sha256"].nbytes
    report["all_array_bytes"] += arrays["geometry_sha256"].nbytes
    report["cost"] = {
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "wall_ms": (time.perf_counter() - wall) * 1000,
        "load_before": load,
        "process_peak_bytes": peak_rss_bytes(),
    }
    if args.trace:
        gc.collect()
        tracemalloc.start()
        replay, _ = cook(args.input, set(args.cells), geometry, at)
        _, peak = tracemalloc.get_traced_memory()
        live = sum(s.count for s in tracemalloc.take_snapshot().statistics("filename"))
        tracemalloc.stop()
        if not all(arrays[k].tobytes() == replay[k].tobytes() for k in replay):
            raise ValueError("traced material replay changed output words")
        report["traced_replay"] = {
            "same_output_words": True,
            "python_traced_peak_bytes": peak,
            "python_traced_live_blocks": live,
            "scope": "Whole second source-read/build, not cumulative allocations, GEOS heap or physical copy traffic; both outputs remain live.",
        }
    report["producer"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip()
    np.savez(args.output / "appearance.npz", **arrays)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in ("supported_groups", "all_groups", "all_array_bytes", "cost")
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    main()
