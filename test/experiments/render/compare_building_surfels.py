"""Bounded source surface hull versus existing root prism and full original remainder."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from build_building_surfels import read_surfels
from building_mass_material_image import evaluate
from building_mass_materials import policy
from building_material_reference import face_materials
from building_surfel_image import render, tangent_axes
from building_tree_load import load_cut, read_tree
from building_tree_queries import view
from building_tree_reference import reference
from building_tree_transfer import transfers
from compare_building_hierarchy import complete, write_images
from compare_building_source_models import contact
from compare_directed_groups import retained_subset
from compare_mass_appearance import errors, read
from process_memory import peak_rss_bytes
from triangle_image_reference import projected_bounds


def compare_query(original, fields, product, material, surfels, at, camera, tables):
    candidate, storage = complete(original, product)
    _, u, v = tangent_axes(surfels["normal"])
    radius = np.sqrt(2) * float(surfels["spacing"])
    support = np.vstack(
        [
            surfels["positions"] + radius * (a * u + b * v)
            for a in (-1, 1)
            for b in (-1, 1)
        ]
    )
    bounds = projected_bounds(
        np.vstack((original["positions"], candidate["positions"], support)), camera
    )
    box = np.array(
        [
            np.maximum(np.floor(bounds[0]) - 1, [0, 0]),
            np.minimum(np.ceil(bounds[1]) + 1, camera[3:]),
        ],
        int,
    )
    images, work = {}, {}
    images["original"], work["original"] = evaluate(
        original, original, fields, material, product, camera, box, tables
    )
    images["parent_uniform"], work["parent_uniform"] = transfers(
        candidate, original, fields, product, material, camera, box, tables, 16
    )
    rest = retained_subset(original, surfels["needs_sources"])
    if not np.array_equal(surfels["needs_sources"], product["needs_sources"]):
        raise ValueError("both candidates must pay the identical original remainder")
    images["surface_samples"], work["surface_samples"] = render(
        surfels, rest, at, camera, box, tables
    )
    return (
        images,
        box,
        {
            "roi": box.tolist(),
            "errors_vs_original": {
                n: errors(img, images["original"])
                for n, img in images.items()
                if n != "original"
            },
            "work": work,
            "parent_storage": storage,
            "resident_surface_array_bytes": sum(v.nbytes for v in surfels.values()),
        },
    )


def run_case(args, cell, at, tables):
    root = args.input / f"cell-{cell}"
    case, tree = json.loads((root / "report.json").read_text()), read_tree(root)
    original, provenance = reference(
        args.source,
        cell,
        tree,
        native_input=args.native_input,
        native_cell=args.native_cell,
    )
    fields, needs = face_materials(original, at)
    if needs:
        raise ValueError("complete original material reference required")
    surfels = read_surfels(
        args.surfels / f"cell-{cell}.npz", root / case["node_files"]["0"], tree, at
    )
    output = args.output / f"cell-{cell}"
    output.mkdir()
    reports, panels = [], []
    for query in case["queries"][1::2]:
        if query["nodes"] != [0]:
            raise ValueError("existing root-boundary views required")
        product, material, _ = load_cut(root, case, tree, query, at)
        camera = view(query | {"eye": query["render_eye"]})
        images, box, report = compare_query(
            original, fields, product, material, surfels, at, camera, tables
        )
        write_images(output, query["name"].split("-")[0], images, {}, camera, box)
        reports.append(report)
        panels.append((images, box))
    contact(panels, output)
    return {"cell": cell, "reference": provenance, "views": reports}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "source", "surfels", "output"):
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
    reports = [run_case(args, cell, policy(), tables) for cell in args.cells]
    result = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cells": reports,
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "peak_bytes": peak_rss_bytes(),
        },
        "scope": "Fixed four source-tree boundary views,1280x720,matchedSS16. Same original remainder, full original references and material policy. Source hull has bounded samples but may omit thin/interior surfaces; no inferred invisibility or global error/Ready/shadow/native/world/device qualification.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["cost"]))


if __name__ == "__main__":
    main()
