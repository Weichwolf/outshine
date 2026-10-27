"""Replay the four existing regional queries at one pixel-center sample."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from building_mass_materials import policy
from building_material_reference import face_materials
from building_tree_load import load_cut, read_tree
from building_tree_queries import view
from compare_building_brdf import fitted_cut
from compare_building_hierarchy import complete, write_images
from compare_building_parent_shading import render_models
from compare_building_regional_hierarchy import rest_price
from compare_mass_appearance import read
from fit_building_brdf import validate_fit_receipt
from native_building_cell_reference import load_cells
from triangle_image_reference import projected_bounds


def reference_query(args, case, tree, query, original, fields, provenance, tables):
    product, full, loading = load_cut(args.input, case, tree, query, policy())
    two, fitted_bytes = fitted_cut(args.input, args.fits / "2", case, query, full, 2)
    mesh, geometry = complete(original, product)
    camera = view(query)
    bounds = projected_bounds(
        np.vstack((original["positions"], mesh["positions"])), camera
    )
    box = np.array(
        [
            np.maximum(np.floor(bounds[0]) - 1, [0, 0]),
            np.minimum(np.ceil(bounds[1]) + 1, camera[3:]),
        ],
        int,
    )
    if np.any(box[1] <= box[0]) or np.prod(box[1] - box[0]) > 1048576:
        raise ValueError("bounded visible existing-query ROI required")
    images, work = render_models(
        original,
        fields,
        product,
        {"full": full, "two": two},
        mesh,
        camera,
        box,
        tables,
        1,
    )
    for image in images.values():
        if not all(np.isfinite(value).all() for value in image.values()):
            raise ValueError("finite complete C/T pixel fields required")
        if np.any((image["T"] < 0) | (image["T"] > 1)):
            raise ValueError("bounded pixel transmittance required")
    write_images(args.output, query["name"], images, {}, camera, box)
    return {
        "query": query,
        "roi": box.tolist(),
        "sample_rate": 1,
        "original_triangles": len(original["indices"]),
        "parent_triangles": len(product["indices"]),
        "geometry": geometry,
        "original_rest": rest_price(
            original, product, provenance["unresolved_originals"], fields=fields
        ),
        "loading": loading,
        "dense_two_lobe_weight_bytes": fitted_bytes,
        "work": work,
        "full_two_transmittance_word_equal": (
            images["full"]["T"].tobytes() == images["two"]["T"].tobytes()
        ),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "fits", "source", "controls", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    case = json.loads((args.input / "report.json").read_text())
    names = tuple(query["name"] for query in case["queries"])
    if names != ("az0-64000", "az0-240000", "az1-64000", "az1-240000"):
        raise ValueError("the four existing regional control queries are required")
    tables_path = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    validate_fit_receipt(args.fits / "2", 2, tables_path)
    tree = read_tree(args.input)
    original, provenance = load_cells(
        args.controls, args.source, set(case["cells"]), tree
    )
    fields, unsupported = face_materials(original, policy())
    if unsupported:
        raise ValueError("all available original material faces must be qualified")
    args.output.mkdir(parents=True, exist_ok=False)
    tables = read(tables_path)
    comparisons = [
        reference_query(args, case, tree, query, original, fields, provenance, tables)
        for query in case["queries"]
    ]
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "provenance": provenance,
        "comparisons": comparisons,
        "scope": "One center sample per pixel on the four unchanged regional controls, existing first-hit-per-group/full/two-lobe C/T model and opaque available original rest. Existing NPZ fields and display-only PNGs, no new product format. Source3825/3827 remain required/unpriced; no background/Ready/depth/occluder/native-PBR/device/p99 admission. Reference GGX counts are not GPU times and are not divided supersample counts.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    for row in comparisons:
        print(
            row["query"]["name"],
            row["roi"],
            row["full_two_transmittance_word_equal"],
            flush=True,
        )


if __name__ == "__main__":
    main()
