"""Local versus uniform parent coverage at the existing four root-boundary views."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from build_building_coverage import field_cut
from building_mass_materials import policy
from building_material_reference import face_materials
from building_tree_load import read_tree
from building_tree_queries import view
from building_tree_reference import reference
from building_tree_transfer import transfers
from compare_building_hierarchy import complete, write_images
from compare_building_transition import endpoints, render_endpoints
from compare_mass_appearance import errors, read
from PIL import Image, ImageDraw
from process_memory import peak_rss_bytes
from urban_parent_light import display


def compare_pair(
    root, field_root, case, tree, queries, original, fields, at, tables, output
):
    products, materials, _, resident = endpoints(root, case, tree, queries, at)
    camera = view(queries[0] | {"eye": queries[0]["render_eye"]})
    images, coverage, box, storage, work = render_endpoints(
        original, fields, products, materials, camera, tables
    )
    results = {}
    for endpoint, query in zip(("finer", "coarser"), queries, strict=True):
        product = products[endpoint]
        local, loading = field_cut(
            root, field_root, case, query, len(materials[endpoint]["albedo"])
        )
        mesh, _ = complete(original, product)
        image, counts = transfers(
            mesh,
            original,
            fields,
            product,
            materials[endpoint],
            camera,
            box,
            tables,
            16,
            coverage=local,
        )
        name = "local_" + endpoint
        images[name] = image
        results[endpoint] = {
            "uniform_vs_original": errors(
                images["fractional_" + endpoint], images["original"]
            ),
            "local_vs_original": errors(image, images["original"]),
            "local_vs_uniform": errors(image, images["fractional_" + endpoint]),
            "work": counts,
            "loading": loading,
        }
    name = queries[0]["name"].split("-")[0]
    write_images(
        output,
        name,
        {k: images[k] for k in ("original", "fractional_coarser", "local_coarser")},
        coverage,
        camera,
        box,
    )
    np.savez(
        output / (name + "-all-models.npz"),
        **{n + "__" + k: v for n, image in images.items() for k, v in image.items()},
    )
    return {
        "distance_m": queries[0]["render_distance_m"],
        "roi": box.tolist(),
        "both_ready_parent_bytes": resident,
        "storage": storage,
        "baseline_work": work,
        "endpoints": results,
    }, images


def contact(panels, output):
    page = Image.new("RGB", (1200, 1000), (24, 24, 24))
    draw = ImageDraw.Draw(page)
    names = ("original", "fractional_coarser", "local_coarser")
    for column, (report, images) in enumerate(panels):
        draw.text(
            (column * 600 + 8, 4),
            f"{report['distance_m'] / 1000:.3f}km, same1280x720 view, SS16",
            fill="white",
        )
        pixels = [display(images[n]["C"] + 0.1 * images[n]["T"]) for n in names]
        pixels.append(
            np.clip(
                abs(pixels[2].astype(int) - pixels[0].astype(int)) * 8, 0, 255
            ).astype(np.uint8)
        )
        for row, (label, image) in enumerate(
            zip((*names, "8x local vs original display error"), pixels, strict=True)
        ):
            h, w = image.shape[:2]
            zoom = max(1, min(32, 550 // w, 170 // h))
            y = 40 + row * 230
            page.paste(
                Image.fromarray(image).resize(
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
    root, field_root = args.input / f"cell-{cell}", args.fields / f"cell-{cell}"
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
        raise ValueError("complete original material reference required")
    output = args.output / f"cell-{cell}"
    output.mkdir()
    panels = [
        compare_pair(
            root,
            field_root,
            case,
            tree,
            case["queries"][start : start + 2],
            original,
            fields,
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
    for name in ("input", "source", "fields", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--native-input", type=Path)
    parser.add_argument("--native-cell", type=int)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    wall, cpu = time.perf_counter(), time.process_time()
    source_receipt = json.loads((args.input / "report.json").read_text())
    field_receipt = json.loads((args.fields / "report.json").read_text())
    if (
        source_receipt["source_archive_sha256"]
        != field_receipt["source_archive_sha256"]
    ):
        raise ValueError("field and geometry source archives differ")
    table = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    reports = [run_case(args, cell, policy(), read(table)) for cell in args.cells]
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
        "scope": "Same four source-tree root-boundary views,1280x720/SS16, unchanged geometry/materials/original remainder. Only source coverage changes. First-hit/side-envelope/independent-layer approximations persist. Extra field bytes/hash reads charged, no native binding/cache/device/world/occluder/shadow qualification.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["cost"]))


if __name__ == "__main__":
    main()
