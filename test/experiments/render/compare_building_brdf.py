"""Material-only one/two/full comparisons on unchanged source-tree boundary views."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from building_mass_material_image import evaluate
from building_mass_materials import policy
from building_material_reference import face_materials
from building_tree_load import read_tree
from building_tree_queries import view
from building_tree_reference import reference
from building_tree_transfer import transfers
from compare_building_hierarchy import complete, write_images
from compare_building_transition import endpoints, render_endpoints
from compare_mass_appearance import errors, read
from fit_building_brdf import read_fit, read_material, validate_fit_receipt
from PIL import Image, ImageDraw
from process_memory import peak_rss_bytes
from urban_parent_light import display


def fitted_cut(root, fit_root, case, query, original, budget):
    arrays, bytes_read = [], 0
    for node in query["nodes"]:
        name = case["node_files"].get(str(node))
        if name is None:
            continue
        source = root / name
        shape = read_material(source)["m__roughness_weights"].shape
        fitted = read_fit(fit_root / name, source, shape, budget)
        arrays.append(fitted)
        bytes_read += fitted.nbytes
    weights = np.concatenate(arrays) if arrays else original["roughness_weights"][:0]
    if weights.shape != original["roughness_weights"].shape:
        raise ValueError("all original parent material groups must remain present")
    return original | {"roughness_weights": weights}, bytes_read


def compare_query(
    root, case, tree, queries, original, fields, fit_roots, at, tables, output
):
    products, materials, _, _ = endpoints(root, case, tree, queries, at)
    camera = view(queries[0] | {"eye": queries[0]["render_eye"]})
    images, coverage, box, storage, work = render_endpoints(
        original, fields, products, materials, camera, tables
    )
    report = {
        "distance_m": queries[0]["render_distance_m"],
        "roi": box.tolist(),
        "storage": storage,
        "baseline_work": work,
        "variants": {},
    }
    meshes = {name: complete(original, p)[0] for name, p in products.items()}
    for variant, fit_root in fit_roots.items():
        budget = 1 if variant == "one" else 2
        results = {}
        for endpoint, query in zip(("finer", "coarser"), queries, strict=True):
            fitted, read_bytes = fitted_cut(
                root, fit_root, case, query, materials[endpoint], budget
            )
            for model in ("opaque", "fractional"):
                name = model + "_" + endpoint
                if model == "opaque":
                    image, counters = evaluate(
                        meshes[endpoint],
                        original,
                        fields,
                        fitted,
                        products[endpoint],
                        camera,
                        box,
                        tables,
                        parent=True,
                        supersample=16,
                    )
                else:
                    image, counters = transfers(
                        meshes[endpoint],
                        original,
                        fields,
                        products[endpoint],
                        fitted,
                        camera,
                        box,
                        tables,
                        16,
                    )
                np.testing.assert_array_equal(image["T"], images[name]["T"])
                images[variant + "_" + name] = image
                results[name] = {
                    "vs_full_mixture": errors(image, images[name]),
                    "vs_original": errors(image, images["original"]),
                    "work": counters,
                    "weight_bytes_read": read_bytes,
                }
        report["variants"][variant] = results
    name = queries[0]["name"].split("-")[0]
    write_images(
        output,
        name,
        {
            n: img
            for n, img in images.items()
            if n == "original" or n.endswith("fractional_coarser")
        },
        coverage,
        camera,
        box,
    )
    np.savez(
        output / (name + "-all-models.npz"),
        **{n + "__" + k: v for n, img in images.items() for k, v in img.items()},
    )
    return report, images


def contact(panels, output):
    page = Image.new("RGB", (1200, 1210), (24, 24, 24))
    draw = ImageDraw.Draw(page)
    for column, (report, images) in enumerate(panels):
        draw.text(
            (column * 600 + 8, 4),
            f"{report['distance_m'] / 1000:.3f}km, same1280x720 view, SS16",
            fill="white",
        )
        names = (
            "original",
            "fractional_coarser",
            "one_fractional_coarser",
            "two_fractional_coarser",
        )
        rgb = [display(images[n]["C"] + 0.1 * images[n]["T"]) for n in names]
        rgb.append(
            np.clip(abs(rgb[3].astype(int) - rgb[1].astype(int)) * 8, 0, 255).astype(
                np.uint8
            )
        )
        for row, (label, pixels) in enumerate(
            zip((*names, "8x display difference: two vs full"), rgb, strict=True)
        ):
            h, w = pixels.shape[:2]
            zoom = max(1, min(32, 550 // w, 170 // h))
            y = 40 + row * 230
            page.paste(
                Image.fromarray(pixels).resize(
                    (w * zoom, h * zoom), Image.Resampling.NEAREST
                ),
                (column * 600 + 8, y),
            )
            draw.text(
                (column * 600 + 8, y - 18),
                f"{label} / ROI{w}x{h} / zoom{zoom}x",
                fill="white",
            )
    page.save(output / "comparison.png")


def run_case(args, cell, at, tables):
    root = args.input / f"cell-{cell}"
    case = json.loads((root / "report.json").read_text())
    full_tree, tree = read_tree(root), read_tree(root, lineage=False)
    original, provenance = reference(
        args.source,
        cell,
        full_tree,
        native_input=args.native_input,
        native_cell=args.native_cell,
    )
    fields, needs = face_materials(original, at)
    if needs:
        raise ValueError("complete source-material reference required")
    output = args.output / f"cell-{cell}"
    output.mkdir()
    roots = {"one": args.one / f"cell-{cell}", "two": args.two / f"cell-{cell}"}
    panels = [
        compare_query(
            root,
            case,
            tree,
            case["queries"][start : start + 2],
            original,
            fields,
            roots,
            at,
            tables,
            output,
        )
        for start in (0, 2)
    ]
    contact(panels, output)
    return {"cell": cell, "reference": provenance, "views": [p[0] for p in panels]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "source", "one", "two", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--native-input", type=Path)
    parser.add_argument("--native-cell", type=int)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    wall, cpu = time.perf_counter(), time.process_time()
    table_path = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    for budget, path in ((1, args.one), (2, args.two)):
        validate_fit_receipt(path, budget, table_path)
    reports = [run_case(args, cell, policy(), read(table_path)) for cell in args.cells]
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
        "scope": "Same source-node geometry, palette, albedo, original remainder, camera and SS16. Only material weights differ; T must be word-equal. One/two/full mixture, both opaque/fractional and finer/coarser. Counts are CPU reference GGX work, not native GPU/IO/device savings. Dense weight layout and current source-tree model defects remain; no shadow/weather/night/transition/world/native admission.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"cells": len(reports), "cost": result["cost"]}))


if __name__ == "__main__":
    main()
