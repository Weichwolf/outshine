"""Complete original-rest GGX/coverage checks on multiple distances and two azimuths."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from building_mass_material_image import evaluate
from building_mass_materials import policy
from building_material_reference import face_materials
from building_tree_load import load_cut, read_tree
from building_tree_queries import view
from building_tree_reference import reference
from building_tree_transfer import transfers
from compare_directed_groups import retained_subset
from compare_group_coverage import front_source
from compare_mass_appearance import errors, read
from PIL import Image, ImageDraw
from process_memory import peak_rss_bytes
from source_plane_mesh import factorized_positions
from triangle_coverage_reference import integrate
from triangle_image_reference import projected_bounds
from urban_parent_light import display


def complete(original, product):
    rest = retained_subset(original, product["needs_sources"])
    candidate = {
        "positions": np.vstack(
            (product["positions"], rest["positions"].astype(np.float32))
        ),
        "indices": np.vstack(
            (product["indices"], rest["indices"] + len(product["positions"]))
        ),
    }
    packed = factorized_positions(candidate)
    rest_fields = sum(
        v.nbytes
        for k, v in rest.items()
        if k not in ("positions", "indices", "vertex_normal", "binding")
    )
    return candidate, {
        "complete_triangles": len(candidate["indices"]),
        "complete_geometry_bytes": sum(v.nbytes for v in packed.values()),
        "original_remainder_triangles": len(rest["indices"]),
        "original_remainder_decoded_field_bytes": rest_fields,
    }


def continuous(source, camera, box):
    width, height = map(int, box[1] - box[0])
    result = np.zeros((height, width))
    oriented, _ = front_source(source, camera[0])
    tiles = 0
    for y in range(0, height, 128):
        for x in range(0, width, 128):
            w, h = min(128, width - x), min(128, height - y)
            tile = np.array([box[0] + [x, y], box[0] + [x + w, y + h]])
            area, _, _ = integrate(oriented, camera, tile)
            result[y : y + h, x : x + w] = area
            tiles += 1
    return result, {"coverage_tiles": tiles, "tile_size_limit": [128, 128]}


def quadrature_change(
    original,
    candidate,
    fields,
    product,
    material,
    camera,
    box,
    tables,
    images,
    supersample,
):
    refined = {}
    for name, mesh in (("original", original), ("parent", candidate)):
        refined[name], _ = evaluate(
            mesh,
            original,
            fields,
            material,
            product,
            camera,
            box,
            tables,
            parent=name == "parent",
            supersample=2 * supersample,
        )
    refined["fractional"], _ = transfers(
        candidate,
        original,
        fields,
        product,
        material,
        camera,
        box,
        tables,
        2 * supersample,
    )
    return {
        "sample_grid_per_pixel": [2 * supersample] * 2,
        "change_from_base_grid": {
            name: errors(img, images[name]) for name, img in refined.items()
        },
        "refined_opaque_errors": errors(refined["parent"], refined["original"]),
        "refined_fractional_errors": errors(refined["fractional"], refined["original"]),
        "scope": "Empirical regular-quadrature change on this view only, not a radiance integral or error bound.",
    }


def compare_query(root, case, tree, query, original, fields, at, tables, output):
    started = time.perf_counter()
    product, material, loading = load_cut(root, case, tree, query, at)
    candidate, storage = complete(original, product)
    camera = view(query)
    bounds = projected_bounds(
        np.vstack((original["positions"], candidate["positions"])), camera
    )
    box = np.array(
        [
            np.maximum(np.floor(bounds[0]), [0, 0]),
            np.minimum(np.ceil(bounds[1]), query["resolution"]),
        ],
        int,
    )
    if np.any(box[1] <= box[0]):
        raise ValueError("complete reference must remain visible")
    pixel_count = int(np.prod(box[1] - box[0]))
    supersample = min(
        16, 2 ** max(0, int(np.floor(np.log2(np.sqrt(1048576 / pixel_count)))))
    )
    images, areas, works = {}, {}, {}
    for name, mesh in (("original", original), ("parent", candidate)):
        images[name], works[name] = evaluate(
            mesh,
            original,
            fields,
            material,
            product,
            camera,
            box,
            tables,
            parent=name == "parent",
            supersample=supersample,
        )
        areas[name], work = continuous(mesh, camera, box)
        works[name].update(work)
        delta = abs(1 - images[name]["T"][..., 0] - areas[name])
        works[name].update(
            quadrature_coverage_l1_px2=float(delta.sum()),
            quadrature_maximum_alpha_error=float(delta.max()),
        )
    if len(product["indices"]):
        images["fractional"], works["fractional"] = transfers(
            candidate,
            original,
            fields,
            product,
            material,
            camera,
            box,
            tables,
            supersample,
        )
        control, _ = transfers(
            candidate,
            original,
            fields,
            product,
            material,
            camera,
            box,
            tables,
            supersample,
            opaque=True,
        )
        for key in ("C", "T"):
            np.testing.assert_allclose(
                control[key], images["parent"][key], atol=1e-12, rtol=1e-12
            )
    else:
        images["fractional"] = images["parent"]
        works["fractional"] = {"no_parent_groups": True}
    fractional_error = errors(images["fractional"], images["original"])
    fractional_error["sampled_alpha_vs_continuous_original_l1_px2"] = float(
        abs(1 - images["fractional"]["T"][..., 0] - areas["original"]).sum()
    )
    error = errors(images["parent"], images["original"])
    error.update(
        continuous_coverage_l1_px2=float(
            abs(areas["parent"] - areas["original"]).sum()
        ),
        continuous_maximum_alpha_error=float(
            abs(areas["parent"] - areas["original"]).max()
        ),
        original_coverage_sum_px2=float(areas["original"].sum()),
    )
    name = query["name"]
    refined = (
        quadrature_change(
            original,
            candidate,
            fields,
            product,
            material,
            camera,
            box,
            tables,
            images,
            supersample,
        )
        if query["distance_m"] == 64000
        else None
    )
    write_images(output, name, images, areas, camera, box)
    return {
        "query": query,
        "roi": box.tolist(),
        "radiance_supersample": supersample,
        "errors": error,
        "fractional_errors": fractional_error,
        "quadrature_change": refined,
        "work": works,
        "storage": storage,
        "loading": loading,
        "wall_ms": (time.perf_counter() - started) * 1000,
    }, images


def write_images(output, name, images, areas, camera, box):
    np.savez(
        output / (name + ".npz"),
        **{n + "__" + k: v for n, img in images.items() for k, v in img.items()},
        **{n + "__coverage": a for n, a in areas.items()},
    )
    for n, image in images.items():
        rgb = display(image["C"] + 0.1 * image["T"])
        full = np.full(
            (camera[4], camera[3], 3), display(np.array([0.1] * 3)), np.uint8
        )
        full[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = rgb
        Image.fromarray(full).save(output / f"{name}-{n}.png")


def contact(panels, output, azimuth):
    page = Image.new("RGB", (1200, 1120), (24, 24, 24))
    draw = ImageDraw.Draw(page)
    for column, (report, images) in enumerate(panels):
        original = display(images["original"]["C"] + 0.1 * images["original"]["T"])
        parent = display(images["parent"]["C"] + 0.1 * images["parent"]["T"])
        fractional = display(
            images["fractional"]["C"] + 0.1 * images["fractional"]["T"]
        )
        h, w = original.shape[:2]
        zoom = max(1, min(32, 380 // w, 240 // h))
        for row, rgb in enumerate(
            (
                original,
                parent,
                fractional,
                np.clip(
                    abs(fractional.astype(int) - original.astype(int)) * 4, 0, 255
                ).astype(np.uint8),
            )
        ):
            y = 40 + row * 270
            picture = Image.fromarray(rgb)
            page.paste(
                picture.resize((w * zoom, h * zoom), Image.Resampling.NEAREST),
                (column * 400 + 8, y),
            )
            draw.text(
                (column * 400 + 8, y - 18),
                (
                    "original",
                    "opaque parent + original rest",
                    "fractional parent + original rest",
                    "4x fractional display error",
                )[row],
                fill="white",
            )
        q = report["query"]
        draw.text(
            (column * 400 + 8, 4),
            f"{q['distance_m']}m / ROI {w}x{h} / zoom {zoom}x / SS{report['radiance_supersample']}",
            fill="white",
        )
        draw.text(
            (column * 400 + 8, 1100),
            f"alpha L1 opaque {report['errors']['T_l1_px2']:.4g} / fractional {report['fractional_errors']['T_l1_px2']:.4g}px2",
            fill="white",
        )
    page.save(output / f"comparison-az{azimuth}.png")


def run_case(args, cell, at, tables):
    root = args.input / f"cell-{cell}"
    case = json.loads((root / "report.json").read_text())
    tree = read_tree(root)
    original, provenance = reference(
        args.source,
        cell,
        tree,
        native_input=args.native_input,
        native_cell=args.native_cell,
    )
    if set(np.unique(original["binding"][:, 1])) != set(tree["selected_sources"]):
        raise ValueError("complete original source reference required")
    fields, needs = face_materials(original, at)
    if needs:
        raise ValueError(f"reference has unqualified material faces: {needs[:3]}")
    output = args.output / f"cell-{cell}"
    output.mkdir()
    results = []
    for azimuth in (0, 1):
        panels = []
        for query in case["queries"][azimuth * 3 : azimuth * 3 + 3]:
            report, images = compare_query(
                root, case, tree, query, original, fields, at, tables, output
            )
            results.append(report)
            panels.append((report, images))
        contact(panels, output, azimuth)
    control = factorized_positions(
        {
            "positions": original["positions"].astype(np.float32),
            "indices": original["indices"],
        }
    )
    report = {
        "reference": provenance,
        "original_triangles": len(original["indices"]),
        "original_geometry_control_bytes": sum(v.nbytes for v in control.values()),
        "original_decoded_field_bytes": sum(
            v.nbytes
            for k, v in original.items()
            if k not in ("positions", "indices", "vertex_normal", "binding")
        ),
        "queries": results,
    }
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
    at = policy()
    tables = read(
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    reports = [run_case(args, cell, at, tables) for cell in args.cells]
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cells": reports,
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "comparison_peak_bytes": peak_rss_bytes(),
        },
        "scope": "Isolated complete-source GGX model, finite regular radiance quadrature (largest power-of-two rate up to16x16 under1048576 ROI samples, identical for both endpoints; no certified radiance integral) and separate GEOS Double coverage. No shadows/IBL/night/emission, captured native material/shading-normal equivalence, transport/free-space/LOD/device/world/p99 admission. Diagnostic assembly copies are not a native hot-path design; originals, tree metadata and fallback fields are explicitly paid.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "cells": [
                    {
                        "original_triangles": r["original_triangles"],
                        "queries": [
                            {
                                "name": q["query"]["name"],
                                "triangles": q["storage"]["complete_triangles"],
                                "coverage_l1": q["errors"][
                                    "continuous_coverage_l1_px2"
                                ],
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
