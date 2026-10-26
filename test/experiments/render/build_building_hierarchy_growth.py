"""Real regional populations: source demand before forms, then paid parent construction."""

import argparse
import json
import os
import subprocess
import time
from pathlib import Path

import numpy as np
from build_building_hierarchy import build_case
from building_mass_materials import policy
from building_parent_buffer import RECIPES
from building_source_tree import build_tree, select_cut, source_bounds, sources_at
from building_tree_queries import queries
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes


def population_cells(plans, seed_cell, count):
    centres = {}
    all_cells = {r["cell"] for r in plans["records"]}
    for cell in all_cells:
        boxes = [
            s["bounds"]
            for r, s in zip(plans["records"], plans["summaries"], strict=True)
            if r["cell"] == cell and s["supported"]
        ]
        if not boxes:
            raise ValueError(
                "every population cell needs an evidenced centre; no unknown cell omitted"
            )
        centres[cell] = np.mean(boxes, axis=(0, 1))
    if (
        seed_cell not in centres
        or not isinstance(count, int)
        or not 1 <= count <= len(centres)
    ):
        raise ValueError("existing seed cell and bounded population count required")
    ordered = sorted(
        centres,
        key=lambda c: (float(np.linalg.norm(centres[c] - centres[seed_cell])), c),
    )
    return set(ordered[:count])


def growth_queries(tree):
    centre = tree["node_bounds"][0].mean(0)
    result = []
    for azimuth, template in enumerate(queries(tree)[::3]):
        for distance in (64000, 240000):
            forward = np.array(template["basis"])[2]
            result.append(
                template
                | {
                    "name": f"az{azimuth}-{distance}",
                    "eye": (centre - forward * distance).tolist(),
                    "distance_m": distance,
                }
            )
    return result


def selection_audit(parent, cells):
    before = parent.form_reads
    cpu = time.process_time()
    ids, bounds, frame = source_bounds(parent.plans, cells)
    tree = build_tree(ids, bounds)
    result = []
    centre = tree["node_bounds"][0].mean(0)
    forward = np.array(queries(tree)[0]["basis"])[2]
    for distance in (16000, 64000, 240000):
        cut, work = select_cut(tree, centre - forward * distance, 900)
        leaves = [
            int(i)
            for n in cut
            if tree["node_sources"][n, 1] == 1
            for i in sources_at(tree, n)
        ]
        result.append(
            {
                "distance_m": distance,
                "cut_nodes": len(cut),
                "selection": work,
                "leaf_original_sources": leaves,
                "unbounded_original_sources": frame["unbounded_sources"].tolist(),
                "scope": "Before parent qualification: original remainder is a lower bound only.",
            }
        )
    if parent.form_reads != before:
        raise ValueError("demand audit must not decode forms")
    return {
        "selected_sources": len(frame["selected_sources"]),
        "bounded_sources": len(ids),
        "tree_nodes": len(tree["node_bounds"]),
        "tree_array_bytes": sum(v.nbytes for v in tree.values()),
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "queries": result,
    }


def original_rest(plans, path, queries):
    output = []
    for query in queries:
        nodes = query["nodes"]
        identities = set()
        for node in nodes:
            file = path / f"node-{node}.npz"
            if file.exists():
                with np.load(file, allow_pickle=False) as data:
                    identities.update(map(int, data["g__needs_sources"]))
        with np.load(path / "tree.npz", allow_pickle=False) as tree:
            identities.update(map(int, tree["unbounded_sources"]))
            for node in nodes:
                if not (path / f"node-{node}.npz").exists():
                    start, count = tree["node_sources"][node]
                    identities.update(
                        map(int, tree["source_order"][start : start + count])
                    )
        if len(identities) != query["original_needs"]:
            raise ValueError(
                "actual complete original remainder must match compiler report"
            )
        known = [i for i in identities if plans["summaries"][i]["supported"]]
        output.append(
            {
                "name": query["name"],
                "original_sources": sorted(identities),
                "known_source_summary_faces": sum(
                    plans["summaries"][i]["faces"] for i in known
                ),
                "unpriced_original_sources": sorted(identities - set(known)),
                "scope": "Summary faces are not native runtime triangles/bytes. Unpriced originals stay required.",
            }
        )
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seed-cell", type=int, required=True)
    parser.add_argument("--cell-counts", type=int, nargs="+", default=[1, 4, 16, 64])
    parser.add_argument("--buffer-path", choices=RECIPES, default="scalar")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    wall, cpu = time.perf_counter(), time.process_time()
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "host_load_before": os.getloadavg(),
        "buffer_path": args.buffer_path,
        "cases": [],
    }
    with open_parent(args.input) as parent:
        report["source_archive_sha256"] = parent.archive_sha256
        for count in args.cell_counts:
            cells = population_cells(parent.plans, args.seed_cell, count)
            audit = selection_audit(parent, cells)
            output = args.output / f"population-{count}"
            print(
                json.dumps(
                    {
                        "started_cells": sorted(cells),
                        "sources": audit["selected_sources"],
                        "form_reads_before": parent.form_reads,
                    }
                ),
                flush=True,
            )
            case = build_case(
                parent,
                cells,
                output,
                policy(),
                make_queries=growth_queries,
                buffer_path=args.buffer_path,
            )
            case["before_form_demand"] = audit
            case["original_rest_audit"] = original_rest(
                parent.plans, output, case["queries"]
            )
            (output / "report.json").write_text(json.dumps(case, indent=2) + "\n")
            report["cases"].append(case)
            (args.output / "report.json").write_text(
                json.dumps(report, indent=2) + "\n"
            )
            print(
                json.dumps(
                    {
                        "completed_cell_count": count,
                        "queries": [
                            {
                                k: q[k]
                                for k in ("name", "proxy_triangles", "original_needs")
                            }
                            for q in case["queries"]
                        ],
                        "cost": case["cost"],
                    }
                ),
                flush=True,
            )
    report["cost"] = {
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "wall_ms": (time.perf_counter() - wall) * 1000,
        "peak_bytes": peak_rss_bytes(),
    }
    report["scope"] = (
        "Same real source parent, nested evidenced cells, unchanged target-span recipe,64/240km parent products before any fine-image oracle.16km demand is metadata-only. Unpriced complete original remainders remain required. No constant image quality/native/whole-world/IO/p99 admission."
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["cost"]))


if __name__ == "__main__":
    main()
