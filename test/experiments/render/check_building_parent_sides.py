"""Check unchanged parent C/T after metadata-selected backface omission."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from building_mass_material_image import evaluate
from building_mass_materials import policy
from building_material_reference import face_materials
from building_parent_sides import prepare_sides, query_sides
from building_tree_load import load_cut, read_tree, render_arrays
from building_tree_queries import view
from building_tree_reference import reference
from building_tree_transfer import transfers
from compare_building_hierarchy import complete, contact, write_images
from compare_mass_appearance import read
from process_memory import peak_rss_bytes


def selected_faces(case, query, ready, prepared):
    selected, offset = [], 0
    for node in query["nodes"]:
        name = case["node_files"].get(str(node))
        if name is not None:
            faces, _ = query_sides(prepared[name], ready[name], query["eye"])
            selected.extend(faces + offset)
            offset += len(ready[name]["g__indices"])
    return np.array(selected, int), offset


def check_cell(args, cell, at, tables):
    root = args.input / f"cell-{cell}"
    case = json.loads((root / "report.json").read_text())
    baseline = args.comparison / f"cell-{cell}"
    report = json.loads((baseline / "report.json").read_text())
    tree = read_tree(root)
    original, _ = reference(
        args.source,
        cell,
        tree,
        native_input=args.native_input,
        native_cell=args.native_cell,
    )
    fields, needs = face_materials(original, at)
    if needs:
        raise ValueError("whole original material reference required")
    ready = {name: render_arrays(root / name) for name in case["node_files"].values()}
    prepared = {name: prepare_sides(data) for name, data in ready.items()}
    output = args.output / f"cell-{cell}"
    output.mkdir()
    checked, panels = [], []
    for q in report["queries"]:
        query = q["query"]
        expected = read(baseline / (query["name"] + ".npz"))
        images = {
            name: {k: expected[name + "__" + k] for k in ("C", "T")}
            for name in ("original", "parent", "fractional")
        }
        selected, count = selected_faces(case, query, ready, prepared)
        if count:
            product, material, _ = load_cut(root, case, tree, query, at)
            if count != len(product["indices"]):
                raise ValueError(
                    "selected side ranges must match the same complete parent cut"
                )
            product = product | {
                "indices": product["indices"][selected],
                "triangle_groups": product["triangle_groups"][selected],
            }
            candidate, _ = complete(original, product)
            camera, box, rate = (
                view(query),
                np.array(q["roi"]),
                q["radiance_supersample"],
            )
            images["parent"], _ = evaluate(
                candidate,
                original,
                fields,
                material,
                product,
                camera,
                box,
                tables,
                parent=True,
                supersample=rate,
            )
            images["fractional"], _ = transfers(
                candidate,
                original,
                fields,
                product,
                material,
                camera,
                box,
                tables,
                rate,
            )
            for name in ("parent", "fractional"):
                for key in ("C", "T"):
                    np.testing.assert_allclose(
                        images[name][key],
                        expected[name + "__" + key],
                        atol=1e-12,
                        rtol=1e-12,
                    )
            write_images(output, query["name"], images, {}, camera, box)
        checked.append(
            {
                "query": query["name"],
                "parent_triangles_before": count,
                "parent_triangles_after": len(selected),
                "maximum_C_T_error": max(
                    float(abs(img[k] - expected[name + "__" + k]).max())
                    for name, img in images.items()
                    for k in ("C", "T")
                ),
            }
        )
        panels.append((q, images))
        if len(panels) == 3:
            contact(panels, output, len(checked) // 3 - 1)
            panels = []
    return {"cell": cell, "queries": checked}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "source", "comparison", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--native-input", type=Path)
    parser.add_argument("--native-cell", type=int)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    wall, cpu = time.perf_counter(), time.process_time()
    tables = read(
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cells": [check_cell(args, cell, policy(), tables) for cell in args.cells],
        "scope": "Finite source-parent C/T controls at the unchanged stored views/rates, opaque and fractional. Original parent order preserved by diagnostic face maps, not an original-source per-pixel ID requirement or a proposed split cache. No world/native/shadow/transition admission.",
    }
    result["cost"] = {
        "cpu_ms": (time.process_time() - cpu) * 1000,
        "wall_ms": (time.perf_counter() - wall) * 1000,
        "peak_bytes": peak_rss_bytes(),
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
