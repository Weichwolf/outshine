"""Fixed six directional parent-side controls with paid descriptor/range arrays."""

import argparse
import json
import subprocess
import time
from pathlib import Path

from building_parent_sides import prepare_sides, query_sides
from building_tree_load import render_arrays
from process_memory import peak_rss_bytes


def measure(root):
    case = json.loads((root / "report.json").read_text())
    arrays = {name: render_arrays(root / name) for name in case["node_files"].values()}
    ready = {name: prepare_sides(data) for name, data in arrays.items()}
    queries = []
    for query in case["queries"]:
        results = []
        for node in query["nodes"]:
            name = case["node_files"].get(str(node))
            if name:
                _, counters = query_sides(ready[name], arrays[name], query["eye"])
                results.append(counters)
        sums = (
            {
                key: sum(row[key] for row in results)
                for key in results[0]
                if key != "margin_m"
            }
            if results
            else {}
        )
        queries.append({"name": query["name"], "nodes": results, "totals": sums})
    return {
        "cell": case["cell"],
        "queries": queries,
        "prepared_parent_products": len(ready),
        "known_preparation_array_bytes": sum(
            v.nbytes
            if hasattr(v, "nbytes")
            else sum(a.nbytes for a in v)
            if isinstance(v, list)
            else 0
            for item in ready.values()
            for v in item.values()
        ),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    wall, cpu = time.perf_counter(), time.process_time()
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_producer": json.loads((args.input / "report.json").read_text())[
            "producer"
        ],
        "cells": [measure(args.input / f"cell-{cell}") for cell in args.cells],
        "scope": "Prepared Double cones on unchanged source-hierarchy parent geometry only; six fixed dominant-normal signs, no group-size/pose sweep or source-lineage requirement. Counts include all unchanged shared XYZ/material/needs/frame/tree arrays,64-byte Double cones and8-byte ranges per nonempty side. Optional source-reordering/NPZ/SSD/native/GPU overhead is not a proven saving; original-remainder cost is unchanged and remains in the source comparison. Diagnostic original-parent face maps and controls are separately counted. This is a raw-array cost model, not an implemented split asset cache. Unknown or degenerate cones keep geometry; every rejection is independently checked against all original parent planes. No source/fine geometry/shader alteration, shadow/occluder/collision/native admission or new standard from these counts.",
    }
    result["cost"] = {
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "wall_ms": (time.perf_counter() - wall) * 1000,
        "peak_bytes": peak_rss_bytes(),
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"cells": len(result["cells"]), "cost": result["cost"]}))


if __name__ == "__main__":
    main()
