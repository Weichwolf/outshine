"""Separate source-reference and group-material losses before changing parent geometry."""

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
from compare_building_hierarchy import complete, write_images
from compare_directed_groups import retained_subset
from compare_mass_appearance import errors, read
from PIL import Image, ImageDraw
from process_memory import peak_rss_bytes
from triangle_image_reference import projected_bounds
from urban_parent_light import display


def joined_sources(analytic, native, retained):
    if np.isin(analytic["binding"][:, 1], retained).any():
        raise ValueError("analytic sources and native remainder must be disjoint")
    rest = retained_subset(native, retained)
    result = {
        k: np.concatenate((analytic[k], rest[k])) for k in analytic if k != "indices"
    }
    result["indices"] = np.vstack(
        (analytic["indices"], rest["indices"] + len(analytic["positions"]))
    )
    if set(np.unique(result["binding"][:, 1])) != set(
        np.unique(native["binding"][:, 1])
    ):
        raise ValueError(
            "analytic model and retained native remainder must cover all sources"
        )
    return result


def material_control(native, node, material, *, semantic):
    owners = np.full(len(native["indices"]), -1, int)
    for group, (begin, end) in enumerate(
        zip(node["group_offsets"][:-1], node["group_offsets"][1:], strict=True)
    ):
        members = node["group_sources"][begin:end]
        selected = np.isin(native["binding"][:, 1], members)
        if np.any(owners[selected] >= 0):
            raise ValueError("source material groups must be disjoint")
        owners[selected] = group
    selected = np.flatnonzero(owners >= 0)
    rest = np.flatnonzero(owners < 0)
    product = {
        "indices": native["indices"][selected],
        "triangle_groups": owners[selected],
        "needs_sources": node["needs_sources"],
    }
    if not np.array_equal(
        np.unique(native["binding"][rest, 1]), np.sort(node["needs_sources"])
    ):
        raise ValueError("material control must preserve the exact native remainder")
    if semantic:
        roles = native["binding"][selected, 4]
        if not np.isin(roles, [0, 1]).all():
            raise ValueError("captured native wall/roof stream required")
        product["triangle_groups"] = 2 * owners[selected] + roles
        material = material | {
            "albedo": np.repeat(material["albedo"].reshape(-1, 3)[:, None], 2, axis=1),
            "roughness_weights": np.repeat(
                material["roughness_weights"].reshape(
                    -1, len(material["roughness_palette"])
                )[:, None],
                2,
                axis=1,
            ),
            "supported": np.repeat(material["supported"], 2),
        }
    candidate = {
        "positions": native["positions"],
        "indices": native["indices"][np.r_[selected, rest]],
    }
    return candidate, product, material


def compare_view(
    native, analytic, node, product, material, fields, analytic_fields, camera, tables
):
    parent, storage = complete(native, product)
    bounds = projected_bounds(
        np.vstack((native["positions"], analytic["positions"], parent["positions"])),
        camera,
    )
    box = np.array(
        [
            np.maximum(np.floor(bounds[0]) - 1, [0, 0]),
            np.minimum(np.ceil(bounds[1]) + 1, camera[3:]),
        ],
        int,
    )
    images, work = {}, {}
    for name, mesh, source_fields in (
        ("native", native, fields),
        ("analytic_with_native_rest", analytic, analytic_fields),
    ):
        images[name], work[name] = evaluate(
            mesh, mesh, source_fields, material, product, camera, box, tables
        )
    for semantic in (False, True):
        name = "native_group_material_" + (
            "semantic_role" if semantic else "normal_role"
        )
        mesh, control, appearance = material_control(
            native, node, material, semantic=semantic
        )
        images[name], work[name] = evaluate(
            mesh, native, fields, appearance, control, camera, box, tables, parent=True
        )
        np.testing.assert_array_equal(images[name]["T"], images["native"]["T"])
    images["parent_uniform"], work["parent_uniform"] = transfers(
        parent, native, fields, product, material, camera, box, tables, 16
    )
    return (
        images,
        box,
        {
            "roi": box.tolist(),
            "errors_vs_native": {
                n: errors(img, images["native"])
                for n, img in images.items()
                if n != "native"
            },
            "normal_vs_semantic_group_material": errors(
                images["native_group_material_normal_role"],
                images["native_group_material_semantic_role"],
            ),
            "normal_semantic_CT_word_equal": all(
                np.array_equal(
                    images["native_group_material_normal_role"][k],
                    images["native_group_material_semantic_role"][k],
                )
                for k in ("C", "T")
            ),
            "work": work,
            "parent_storage": storage,
        },
    )


def contact(panels, output):
    page = Image.new("RGB", (1200, 1200), (24, 24, 24))
    draw = ImageDraw.Draw(page)
    for column, (images, _) in enumerate(panels):
        for row, (name, img) in enumerate(images.items()):
            rgb = display(img["C"] + 0.1 * img["T"])
            h, w = rgb.shape[:2]
            zoom = max(1, min(32, 550 // w, 150 // h))
            y = 35 + row * 235
            page.paste(
                Image.fromarray(rgb).resize(
                    (w * zoom, h * zoom), Image.Resampling.NEAREST
                ),
                (column * 600 + 8, y),
            )
            draw.text(
                (column * 600 + 8, y - 20),
                f"{name}, ROI{w}x{h}, zoom{zoom}x",
                fill="white",
            )
    page.save(output / "comparison.png")


def run(args):
    root = args.input / f"cell-{args.cell}"
    case, tree = json.loads((root / "report.json").read_text()), read_tree(root)
    with np.load(root / case["node_files"]["0"], allow_pickle=False) as archive:
        node = {
            k: archive["g__" + k]
            for k in ("group_sources", "group_offsets", "needs_sources")
        }
    native, provenance = reference(
        args.source,
        args.cell,
        tree,
        native_input=args.native_input,
        native_cell=args.cell,
    )
    frame = tree | {"selected_sources": node["group_sources"]}
    analytic, analytic_provenance = reference(args.source, args.cell, frame)
    analytic = joined_sources(analytic, native, node["needs_sources"])
    at = policy()
    fields, needs = face_materials(native, at)
    analytic_fields, analytic_needs = face_materials(analytic, at)
    if needs or analytic_needs:
        raise ValueError("both complete source material references must qualify")
    tables = read(
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    panels, reports = [], []
    for query in case["queries"][1::2]:
        if query["nodes"] != [0]:
            raise ValueError("fixed root-boundary comparison required")
        product, material, _ = load_cut(root, case, tree, query, at)
        camera = view(query | {"eye": query["render_eye"]})
        images, box, report = compare_view(
            native,
            analytic,
            node,
            product,
            material,
            fields,
            analytic_fields,
            camera,
            tables,
        )
        name = query["name"].split("-")[0]
        write_images(
            args.output,
            name,
            {"original" if k == "native" else k: v for k, v in images.items()},
            {},
            camera,
            box,
        )
        np.savez(
            args.output / (name + "-models.npz"),
            **{
                n + "__" + k: v for n, image in images.items() for k, v in image.items()
            },
        )
        panels.append((images, box))
        reports.append(report)
    contact(panels, args.output)
    return {
        "cell": args.cell,
        "native_reference": provenance,
        "analytic_reference": analytic_provenance,
        "analytic_sources": len(node["group_sources"]),
        "retained_native_sources": node["needs_sources"].tolist(),
        "reference_array_bytes": {
            "native": sum(v.nbytes for v in native.values()),
            "analytic_with_native_rest": sum(v.nbytes for v in analytic.values()),
        },
        "views": reports,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "source", "native-input", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cell", type=int, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    wall, cpu = time.perf_counter(), time.process_time()
    result = run(args)
    result.update(
        {
            "producer": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], text=True
            ).strip(),
            "cost": {
                "cpu_ms": (time.process_time() - cpu) * 1000,
                "wall_ms": (time.perf_counter() - wall) * 1000,
                "peak_bytes": peak_rss_bytes(),
            },
            "scope": "Two existing1280x720/SS16 root views. Analytic geometry only for already qualified grouped sources; exact native remainder paid. Native geometry/normals preserved in two material-only controls; semantic wall/roof versus current normal classification separated. No new parent/normal/LOD/native representation or equivalence guarantee.",
        }
    )
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["cost"]))


if __name__ == "__main__":
    main()
