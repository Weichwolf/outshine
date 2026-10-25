"""Generate demanded reusable node products for declared multi-distance diagnostics."""

import argparse
import hashlib
import json
import os
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from building_mass_appearance import group_materials, source_measure
from building_mass_materials import policy
from building_source_tree import build_tree, select_cut, source_bounds, sources_at
from building_tree_products import build_cut, build_node, prepare_sources
from building_tree_queries import queries
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes

RENDER_GEOMETRY = (
    "positions",
    "indices",
    "triangle_groups",
    "needs_sources",
    "basis",
    "anchor",
    "group_coverage",
    "tree_sha256",
)
RENDER_MATERIAL = (
    "albedo",
    "roughness_weights",
    "roughness_palette",
    "supported",
    "policy_sha256",
)


def measure(forms, wanted, frame, at):
    values, needs, patches, calls = {}, [], 0, 0
    for identity in sorted(wanted):
        try:
            values[identity], p, c = source_measure(
                identity, forms[identity], frame, at
            )
            patches += p
            calls += c
        except (ValueError, ArithmeticError, shapely.errors.GEOSException) as error:
            needs.append({"source": identity, "reason": str(error)})
    return values, {
        "measured_sources_once": len(values),
        "material_patches": patches,
        "material_evaluations": calls,
        "source_needs": needs,
    }


def write_nodes(root, tree, nodes, ready, frame, values, at):
    files, report = {}, []
    for node in sorted(nodes):
        if not ready[node][0]:
            continue
        local_frame = frame | {"selected_sources": sources_at(tree, node)}
        product, _ = build_cut(tree, [node], {}, local_frame, ready_nodes=ready)
        material = group_materials(values, product, at)
        if not material["supported"].all():
            ready[node] = (
                [],
                [
                    {
                        "node": node,
                        "reason": "whole node retained as original: incomplete material measures",
                    }
                ],
            )
            continue
        payload = {"g__" + k: v for k, v in product.items()} | {
            "m__" + k: v for k, v in material.items()
        }
        path = root / f"node-{node}.npz"
        np.savez(path, **payload)
        files[node] = path.name
        render = sum(product[k].nbytes for k in RENDER_GEOMETRY) + sum(
            material[k].nbytes for k in RENDER_MATERIAL
        )
        report.append(
            {
                "node": node,
                "file": path.name,
                "raw_all_bytes": sum(v.nbytes for v in payload.values()),
                "raw_render_bytes": render,
                "container_bytes": path.stat().st_size,
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            }
        )
    return files, report


def case(parent, cell, output, at, *, make_queries=queries):
    wall, cpu = time.perf_counter(), time.process_time()
    output.mkdir()
    before_reads = parent.form_reads
    ids, bounds, frame = source_bounds(parent.plans, {cell})
    tree = build_tree(ids, bounds)
    digest = hashlib.sha256(b"building-tree-recipe-1" + parent.archive_sha256.encode())
    for array in (*tree.values(), frame["basis"], frame["anchor"]):
        digest.update(array.tobytes())
    frame["tree_sha256"] = np.frombuffer(digest.digest(), np.uint8).copy()
    poses = make_queries(tree)
    cuts = []
    for pose in poses:
        cut, work = select_cut(tree, pose["eye"], pose["focal_px"])
        cuts.append(cut)
        pose.update(nodes=cut.tolist(), selection=work)
    if parent.form_reads != before_reads:
        raise ValueError("detail demand must precede source form reads")
    requested = {
        int(i)
        for cut in cuts
        for n in cut
        if tree["node_sources"][n, 1] > 1
        for i in sources_at(tree, n)
    }
    masses, forms, read = prepare_sources(parent, {cell}, requested, frame)
    demanded = {int(n) for cut in cuts for n in cut}
    parents = {n for n in demanded if tree["node_sources"][n, 1] > 1}
    ready = {
        n: build_node(tree, n, masses) if n in parents else ([], [])
        for n in sorted(demanded)
    }
    wanted = {
        int(i)
        for groups, _ in ready.values()
        for _, _, work in groups
        for i in work["sources"]
    }
    values, material_work = measure(forms, wanted, frame, at)
    files, node_work = write_nodes(output, tree, set(ready), ready, frame, values, at)
    for pose, cut in zip(poses, cuts, strict=True):
        product, work = build_cut(tree, cut, {}, frame, ready_nodes=ready)
        pose.update(
            grouped_sources=len(product["group_sources"]),
            original_needs=len(product["needs_sources"]),
            proxy_triangles=len(product["indices"]),
            raw_cut_geometry_bytes=sum(v.nbytes for v in product.values()),
            reused_nodes=work["reused_node_products"],
            new_nodes=work["new_node_products"],
        )
    np.savez(output / "tree.npz", **tree, **frame)
    report = {
        "cell": cell,
        "sources": len(frame["selected_sources"]),
        "node_files": files,
        "node_products": node_work,
        "queries": poses,
        "tree_nodes": len(tree["node_bounds"]),
        "tree_array_bytes": sum(v.nbytes for v in tree.values()),
        "source_read": read,
        "material_work": material_work,
        "generated_nodes_once": len(parents),
        "generated_mesh_groups_once": sum(len(g) for g, _ in ready.values()),
        "avoided_node_generations": sum(sum(int(n) in parents for n in c) for c in cuts)
        - len(parents),
        "all_query_nodes": sum(len(c) for c in cuts),
        "form_reads_for_selection": 0,
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "peak_bytes": peak_rss_bytes(),
        },
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    at = policy()
    wall, cpu = time.perf_counter(), time.process_time()
    load = os.getloadavg()
    with open_parent(args.input) as parent:
        reports = [
            case(parent, cell, args.output / f"cell-{cell}", at) for cell in args.cells
        ]
        digest = parent.archive_sha256
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_archive_sha256": digest,
        "cells": reports,
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "load_before": load,
            "peak_bytes": peak_rss_bytes(),
        },
        "scope": "Bounded Python source-index hierarchy and demanded node products. Camera data only in diagnostics; assets contain no view/light variants. No original fine mesh input, shared native/cache API, certified image bound or world/device admission. Render-only load omits lineage arrays; complete original remainder remains mandatory.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "cells": [
                    {
                        "cell": r["cell"],
                        "queries": [
                            {
                                k: q[k]
                                for k in ("name", "proxy_triangles", "original_needs")
                            }
                            for q in r["queries"]
                        ],
                    }
                    for r in reports
                ],
                "cost": report["cost"],
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    main()
