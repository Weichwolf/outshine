"""Fresh-process render-only reads, resident set differences and unchanged turns."""

import argparse
import json
import time
from pathlib import Path

from building_mass_materials import policy
from building_tree_load import load_cut, read_tree
from process_memory import peak_rss_bytes


def probe(root, at):
    tree = read_tree(root, lineage=False)
    initial_bytes = sum(v.nbytes for v in tree.values())
    case = json.loads((root / "report.json").read_text())
    # Near -> middle -> turn -> far -> turn -> return; no permanent growth.
    ready, results = {}, []
    for index in (0, 1, 4, 2, 5, 0):
        query = case["queries"][index]
        before = set(ready)
        geometry, material, work = load_cut(
            root, case, tree, query, at, ready_arrays=ready
        )
        needed = set(work["parent_files"])
        evicted = sorted(before - needed)
        for key in evicted:
            del ready[key]
        if set(ready) != needed:
            raise ValueError("resident parents must equal the current declared demand")
        results.append(
            {
                "query": query["name"],
                "loading": work,
                "evicted_parent_files": evicted,
                "resident_parent_arrays_bytes": sum(
                    v.nbytes for data in ready.values() for v in data.values()
                ),
                "diagnostic_assembly_arrays_bytes": sum(
                    v.nbytes for v in geometry.values()
                )
                + sum(v.nbytes for v in material.values()),
                "original_sources_needed": len(geometry["needs_sources"]),
            }
        )
    return {
        "cell": case["cell"],
        "initial_tree_metadata_bytes": initial_bytes,
        "queries": results,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    wall, cpu = time.perf_counter(), time.process_time()
    peak_before = peak_rss_bytes()
    at = policy()
    results = [probe(args.input / f"cell-{cell}", at) for cell in args.cells]
    report = {
        "cells": results,
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "wall_ms": (time.perf_counter() - wall) * 1000,
        "peak_bytes": peak_rss_bytes(),
        "process_high_water_before_probe_bytes": peak_before,
        "scope": "Python NPZ array reads, not physical SSD bytes or native cache admission. Original assets are counted as needs but not loaded here; their measured full cost is in the image comparison. Full tree metadata and lazily read original-source map are paid. No fine source forms, contour, mesh or material generation. Reused arrays still undergo frame/tree/material validation; diagnostic assembly copies remain. Eviction is immediate after each query and proves bounded reference residency only, not asynchronous GPU retirement.",
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "cells": len(results),
                "cpu_ms": report["cpu_ms"],
                "peak_bytes": report["peak_bytes"],
            }
        )
    )


if __name__ == "__main__":
    main()
