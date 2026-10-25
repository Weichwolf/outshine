"""Image/cost comparison with both original-cut and exactly matched-remainder controls."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from build_building_mattes import read_matte, read_prism, response_signatures
from building_mass_material_image import evaluate
from building_mass_materials import policy
from building_material_reference import face_materials
from building_matte_image import quad, render
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
from urban_parent_light import display


def compare_query(
    original,
    fields,
    baseline,
    baseline_material,
    prism,
    material,
    separate,
    merged,
    camera,
    tables,
    *,
    lighting=None,
    fixed_box=None,
):
    lighting = {} if lighting is None else lighting
    if not np.array_equal(
        prism["needs_sources"], separate["needs_sources"]
    ) or not np.array_equal(prism["needs_sources"], merged["needs_sources"]):
        raise ValueError(
            "all paired candidates must pay identical whole-source remainder"
        )
    base_mesh, base_storage = complete(original, baseline)
    prism_mesh, prism_storage = complete(original, prism)
    quads = (
        np.vstack([quad(separate, i) for i in range(len(separate["plane_origin"]))])
        if len(separate["plane_origin"])
        else np.empty((0, 3))
    )
    bounds = projected_bounds(
        np.vstack(
            (
                original["positions"],
                base_mesh["positions"],
                prism_mesh["positions"],
                quads,
            )
        ),
        camera,
    )
    box = np.array(
        [
            np.maximum(np.floor(bounds[0]) - 1, [0, 0]),
            np.minimum(np.ceil(bounds[1]) + 1, camera[3:]),
        ],
        int,
    )
    if fixed_box is not None:
        if np.any(fixed_box[0] > box[0]) or np.any(fixed_box[1] < box[1]):
            raise ValueError("fixed motion ROI must contain every endpoint")
        box = fixed_box
    images, work = {}, {}
    images["original"], work["original"] = evaluate(
        original,
        original,
        fields,
        baseline_material,
        baseline,
        camera,
        box,
        tables,
        **lighting,
    )
    for name, mesh, p, m in (
        ("parent_original_cut", base_mesh, baseline, baseline_material),
        ("parent_matched_rest", prism_mesh, prism, material),
    ):
        images[name], work[name] = transfers(
            mesh,
            original,
            fields,
            p,
            m,
            camera,
            box,
            tables,
            16,
            **lighting,
        )
    for name, data in (("matte_separate", separate), ("matte_merged", merged)):
        images[name], work[name] = render(
            data, original, fields, camera, box, tables, **lighting
        )
    rest = retained_subset(original, prism["needs_sources"])
    report = {
        "roi": box.tolist(),
        "errors_vs_original": {
            n: errors(img, images["original"])
            for n, img in images.items()
            if n != "original"
        },
        "merged_vs_separate": errors(images["matte_merged"], images["matte_separate"]),
        "merged_separate_display_pixels_equal": bool(
            np.array_equal(
                display(
                    images["matte_merged"]["C"] + 0.1 * images["matte_merged"]["T"]
                ),
                display(
                    images["matte_separate"]["C"] + 0.1 * images["matte_separate"]["T"]
                ),
            )
        ),
        "work": work,
        "baseline_storage": base_storage,
        "prism_storage": prism_storage,
        "same_original_remainder_decoded_bytes": sum(v.nbytes for v in rest.values()),
        "same_original_remainder_triangles": len(rest["indices"]),
    }
    return images, box, report


def run_case(args, cell, at, tables):
    root, products = args.input / f"cell-{cell}", args.products / f"cell-{cell}"
    case, tree = json.loads((root / "report.json").read_text()), read_tree(root)
    node = root / case["node_files"]["0"]
    prism, material = read_prism(products / "prism.npz", tree, at)
    separate = read_matte(products / "matte-separate.npz", node, tree, at)
    merged = read_matte(products / "matte-merged.npz", node, tree, at)
    original, provenance = reference(
        args.source,
        cell,
        tree,
        native_input=args.native_input,
        native_cell=args.native_cell,
    )
    fields, needs = face_materials(original, at)
    if needs:
        raise ValueError("complete fine-image material reference required")
    output = args.output / f"cell-{cell}"
    output.mkdir()
    panels, reports = [], []
    for query in case["queries"][1::2]:
        if query["nodes"] != [0]:
            raise ValueError("existing root-boundary view required")
        baseline, baseline_material, _ = load_cut(root, case, tree, query, at)
        camera = view(query | {"eye": query["render_eye"]})
        images, box, report = compare_query(
            original,
            fields,
            baseline,
            baseline_material,
            prism,
            material,
            separate,
            merged,
            camera,
            tables,
        )
        write_images(output, query["name"].split("-")[0], images, {}, camera, box)
        panels.append((images, box))
        reports.append(report)
    contact(panels, output)
    return {
        "cell": cell,
        "reference": provenance,
        "views": reports,
        "decoded_matte_arrays_bytes": {
            "separate": sum(v.nbytes for v in separate.values()),
            "merged": sum(v.nbytes for v in merged.values()),
        },
        "node_container_bytes_read_for_each_binding_hash": node.stat().st_size,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "source", "products", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--native-input", type=Path)
    parser.add_argument("--native-cell", type=int)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    receipt = json.loads((args.products / "report.json").read_text())
    if receipt["response_program_sha256"] != response_signatures():
        raise ValueError("compiled response and current direct-lighting model differ")
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
        "scope": "Same four1280x720/SS16root views. Matched prisms/mattes retain identical whole original sources, separately from original-cut control. Actual normals/full roughness measures, filtered source coverage and deferred opaque stopping. No native/GPU/world/shadow/temporal or global image-error admission.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["cost"]))


if __name__ == "__main__":
    main()
