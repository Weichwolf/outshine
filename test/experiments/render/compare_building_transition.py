"""Same-view source-tree cut transitions and joint ready-product residency."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from building_mass_material_image import evaluate
from building_mass_materials import policy
from building_material_reference import face_materials
from building_source_tree import sources_at
from building_transition import independent_fade_over, mix_layers
from building_tree_load import load_cut, read_tree
from building_tree_queries import view
from building_tree_reference import reference
from building_tree_transfer import transfers
from compare_building_hierarchy import complete, continuous, write_images
from compare_directed_groups import retained_subset
from compare_mass_appearance import errors, read
from PIL import Image, ImageDraw
from process_memory import peak_rss_bytes
from source_plane_mesh import factorized_positions
from triangle_image_reference import projected_bounds
from urban_parent_light import display


def endpoints(root, case, tree, queries, at):
    ready, products, materials, loading = {}, {}, {}, {}
    for name, query in zip(("finer", "coarser"), queries, strict=True):
        p, m, work = load_cut(root, case, tree, query, at, ready_arrays=ready)
        products[name], materials[name], loading[name] = p, m, work
    resident = sum(v.nbytes for data in ready.values() for v in data.values())
    return products, materials, loading, resident


def original_union_cost(original, products):
    needed = np.unique(np.concatenate([p["needs_sources"] for p in products.values()]))
    rest = retained_subset(original, needed)
    control = factorized_positions(
        {"positions": rest["positions"].astype(np.float32), "indices": rest["indices"]}
    )
    return {
        "union_original_sources": needed.tolist(),
        "union_original_triangles": len(rest["indices"]),
        "union_original_XYZ_index_control_bytes": sum(
            v.nbytes for v in control.values()
        ),
        "union_original_decoded_field_bytes": sum(
            v.nbytes
            for k, v in rest.items()
            if k not in ("positions", "indices", "binding", "vertex_normal")
        ),
    }


def render_endpoints(original, fields, products, materials, camera, tables, rate=16):
    meshes, storage = {}, {}
    for name, p in products.items():
        meshes[name], storage[name] = complete(original, p)
    bounds = projected_bounds(
        np.vstack([original["positions"], *(m["positions"] for m in meshes.values())]),
        camera,
    )
    box = np.array(
        [
            np.maximum(np.floor(bounds[0]) - 1, [0, 0]),
            np.minimum(np.ceil(bounds[1]) + 1, camera[3:]),
        ],
        int,
    )
    if np.any(box[1] <= box[0]):
        raise ValueError("complete transition must remain visible")
    images, coverage, work = {}, {}, {}
    first = products["finer"]
    images["original"], work["original"] = evaluate(
        original,
        original,
        fields,
        materials["finer"],
        first,
        camera,
        box,
        tables,
        supersample=rate,
    )
    if rate == 16:
        coverage["original"], _ = continuous(original, camera, box)
    for name, mesh in meshes.items():
        images["opaque_" + name], work["opaque_" + name] = evaluate(
            mesh,
            original,
            fields,
            materials[name],
            products[name],
            camera,
            box,
            tables,
            parent=True,
            supersample=rate,
        )
        images["fractional_" + name], work["fractional_" + name] = transfers(
            mesh,
            original,
            fields,
            products[name],
            materials[name],
            camera,
            box,
            tables,
            rate,
        )
        if rate == 16:
            coverage[name], _ = continuous(mesh, camera, box)
    return images, coverage, box, storage, work


def refine(original, fields, products, materials, camera, tables, images):
    refined, _, _, _, work = render_endpoints(
        original, fields, products, materials, camera, tables, 32
    )
    return {
        "radiance_supersample": 32,
        "empirical_change_from_SS16": {
            name: errors(img, images[name]) for name, img in refined.items()
        },
        "endpoint_jumps": {
            model: errors(refined[model + "_coarser"], refined[model + "_finer"])
            for model in ("opaque", "fractional")
        },
        "endpoint_errors_vs_original": {
            name: errors(img, refined["original"])
            for name, img in refined.items()
            if name != "original"
        },
        "reference_work": work,
        "scope": "Same-view regular-quadrature refinement only, no certified radiance integral or physical/model bound.",
    }


def compare_pair(
    root, case, tree, full_tree, queries, original, fields, at, tables, output
):
    if not np.array_equal(queries[0]["render_eye"], queries[1]["render_eye"]):
        raise ValueError("both endpoints require the same render eye")
    for q in queries:
        covered = np.r_[
            full_tree["unbounded_sources"],
            np.concatenate([sources_at(full_tree, node) for node in q["nodes"]]),
        ]
        np.testing.assert_array_equal(np.sort(covered), full_tree["selected_sources"])
    products, materials, loading, resident = endpoints(root, case, tree, queries, at)
    camera = view(queries[0] | {"eye": queries[0]["render_eye"]})
    images, coverage, box, storage, work = render_endpoints(
        original, fields, products, materials, camera, tables
    )
    report = {
        "distance_m": queries[0]["render_distance_m"],
        "roi": box.tolist(),
        "selection_queries": queries,
        "loading": loading,
        "both_ready_parent_array_bytes": resident,
        "both_diagnostic_assembly_bytes": sum(
            v.nbytes
            for data in [*products.values(), *materials.values()]
            for v in data.values()
        ),
        "storage": storage,
        "remainder": original_union_cost(original, products),
        "radiance_supersample": 16,
        "reference_work": work,
        "quadrature_refinement": refine(
            original, fields, products, materials, camera, tables, images
        ),
        "opaque_continuous_coverage_L1_px2": {
            name: float(abs(area - coverage["original"]).sum())
            for name, area in coverage.items()
        },
        "models": {},
    }
    for model in ("opaque", "fractional"):
        finer, coarser = images[model + "_finer"], images[model + "_coarser"]
        images[model + "_mix"] = mix_layers(finer, coarser, 0.5)
        images[model + "_independent_over"] = independent_fade_over(finer, coarser, 0.5)
        shared = (finer["T"] == 0) & (coarser["T"] == 0)
        report["models"][model] = {
            "shared_fully_opaque_pixels": int(shared.sum()),
            "independent_over_T_on_shared_opaque_pixels": float(
                images[model + "_independent_over"]["T"][shared].max()
            )
            if shared.any()
            else None,
            "extra_T_beyond_endpoint_max_sum_px2": float(
                np.maximum(
                    images[model + "_independent_over"]["T"]
                    - np.maximum(finer["T"], coarser["T"]),
                    0,
                ).sum()
            ),
            "instantaneous_endpoint_jump": errors(coarser, finer),
            "vs_original": {
                name: errors(images[model + "_" + name], images["original"])
                for name in ("finer", "coarser", "mix", "independent_over")
            },
            "independent_over_vs_mix": errors(
                images[model + "_independent_over"], images[model + "_mix"]
            ),
        }
    name = queries[0]["name"].split("-")[0]
    write_images(
        output,
        name,
        {
            "original": images["original"],
            **{
                key: value
                for key, value in images.items()
                if key.startswith("fractional_")
            },
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
    names = (
        "original",
        "fractional_finer",
        "fractional_coarser",
        "fractional_mix",
        "fractional_independent_over",
    )
    for column, (report, images) in enumerate(panels):
        draw.text(
            (column * 600 + 8, 4),
            f"Root cut at {report['distance_m'] / 1000:.3f}km, same1280x720 view, SS16",
            fill="white",
        )
        for row, name in enumerate(names):
            rgb = display(images[name]["C"] + 0.1 * images[name]["T"])
            h, w = rgb.shape[:2]
            zoom = max(1, min(32, 550 // w, 170 // h))
            y = 40 + row * 230
            page.paste(
                Image.fromarray(rgb).resize(
                    (w * zoom, h * zoom), Image.Resampling.NEAREST
                ),
                (column * 600 + 8, y),
            )
            draw.text(
                (column * 600 + 8, y - 18),
                f"{name} / ROI{w}x{h} / {zoom}x display zoom",
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
        raise ValueError(
            "complete original reference must have qualified material fields"
        )
    output = args.output / f"cell-{cell}"
    output.mkdir()
    panels = [
        compare_pair(
            root,
            case,
            tree,
            full_tree,
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
    report = {"cell": cell, "reference": provenance, "views": [p[0] for p in panels]}
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "source", "output"):
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
        "cells": [run_case(args, cell, policy(), tables) for cell in args.cells],
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "peak_bytes": peak_rss_bytes(),
        },
        "scope": "Same source-tree root boundary, actual parent/child products and complete original remainder; camera held identical. Convex complete-layer C/T midpoint vs independently faded layer-over negative control. Both endpoints must already be independently camera-ready for the same source/domain/stop; no Ready inference, caster replacement or new cache/GPU format. Finite SS16/day/direct-GGX controls and GEOS opaque area only; not moving/relit-frame, shadow, native, runtime/device/world/LOD admission. End-model error remains. Simultaneous node arrays, actual original-rest union and diagnostic copies paid; pixel fields are CPU ROI arrays, not a proposed native render-target layout.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"cells": len(result["cells"]), "cost": result["cost"]}))


if __name__ == "__main__":
    main()
