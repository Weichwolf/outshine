"""Same native regional opponent, full available original rest, two parent recipes."""

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
from building_tree_transfer import transfers
from compare_building_hierarchy import complete, continuous, write_images
from compare_building_source_models import contact
from compare_directed_groups import retained_subset
from compare_mass_appearance import errors, read
from native_building_cell_reference import load_cells
from process_memory import peak_rss_bytes
from triangle_image_reference import projected_bounds


def rest_price(original, product, unresolved, *, fields=None):
    unknown = {v["source"] for v in unresolved}
    required = set(map(int, product["needs_sources"]))
    if not unknown <= required:
        raise ValueError("unresolved original geometry must remain required")
    kept = retained_subset(original, sorted(required))
    return {
        "required_original_sources": sorted(required),
        "resolved_original_sources": sorted(required - unknown),
        "unpriced_originals": unresolved,
        "known_original_triangles": len(kept["indices"]),
        "known_decoded_reference_array_bytes": sum(v.nbytes for v in kept.values()),
        "known_reference_material_field_bytes": None
        if fields is None
        else sum(
            v[np.isin(original["binding"][:, 1], product["needs_sources"])].nbytes
            for v in fields.values()
        ),
        "scope": "Only available native geometry is priced. Explicit failed sources have unknown additional geometry/image/cost; CPU reference arrays are not native upload bytes.",
    }


def compare_query(
    query, roots, reports, trees, original, fields, at, tables, unresolved, output
):
    camera = view(query)
    products = {
        k: load_cut(p, reports[k], trees[k], query, at) for k, p in roots.items()
    }
    meshes, prices = {}, {}
    for name, (product, _, _) in products.items():
        meshes[name], _ = complete(original, product)
        prices[name] = rest_price(original, product, unresolved, fields=fields)
    if prices["scalar"] != prices["components"]:
        raise ValueError(
            "both parent paths require the same complete original remainder"
        )
    bounds = projected_bounds(
        np.vstack([original["positions"], *[m["positions"] for m in meshes.values()]]),
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
        raise ValueError("visible complete regional comparison required")
    pixels = int(np.prod(box[1] - box[0]))
    if pixels * 4 > 1048576:
        raise ValueError("bounded shared/refined regional diagnostic ROI required")
    rate = 16
    while pixels * (2 * rate) ** 2 > 1048576 and rate > 1:
        rate //= 2
    product, material, _ = products["scalar"]
    images, work, areas = {}, {}, {}
    images["original"], work["original"] = evaluate(
        original,
        original,
        fields,
        material,
        product,
        camera,
        box,
        tables,
        supersample=rate,
    )
    areas["original"], _ = continuous(original, camera, box)
    for name, (product, material, loading) in products.items():
        mesh = meshes[name]
        images[name + "-opaque"], work[name + "-opaque"] = evaluate(
            mesh,
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
        images[name], work[name] = transfers(
            mesh, original, fields, product, material, camera, box, tables, rate
        )
        areas[name], _ = continuous(mesh, camera, box)
        prices[name]["loading"] = loading
    refined = {}
    product, material, _ = products["scalar"]
    refined["original"], _ = evaluate(
        original,
        original,
        fields,
        material,
        product,
        camera,
        box,
        tables,
        supersample=2 * rate,
    )
    for name, (product, material, _) in products.items():
        refined[name], _ = transfers(
            meshes[name],
            original,
            fields,
            product,
            material,
            camera,
            box,
            tables,
            2 * rate,
        )
    np.savez(
        output / (query["name"] + "-refined.npz"),
        **{k + "__" + f: v for k, image in refined.items() for f, v in image.items()},
    )
    write_images(output, query["name"], images, areas, camera, box)
    report = {
        "query": query,
        "roi": box.tolist(),
        "sample_rate": rate,
        "original_rest": prices,
        "work": work,
        "errors": {
            k: errors(img, images["original"])
            for k, img in images.items()
            if k != "original"
        },
        "quadrature_change": {k: errors(img, images[k]) for k, img in refined.items()},
        "refined_errors": {
            k: errors(img, refined["original"])
            for k, img in refined.items()
            if k != "original"
        },
        "component_vs_scalar": errors(images["components"], images["scalar"]),
    }
    return report, images


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    cpu, wall = time.process_time(), time.perf_counter()
    roots = {"scalar": args.scalar, "components": args.components}
    reports = {k: json.loads((p / "report.json").read_text()) for k, p in roots.items()}
    trees = {k: read_tree(p) for k, p in roots.items()}
    for key, a in trees["scalar"].items():
        b = trees["components"][key]
        if key != "tree_sha256" and (
            a.dtype != b.dtype or a.shape != b.shape or a.tobytes() != b.tobytes()
        ):
            raise ValueError("same complete source hierarchy/frame required")
    original, provenance = load_cells(
        args.controls, args.source, set(reports["scalar"]["cells"]), trees["scalar"]
    )
    at = policy()
    fields, needs = face_materials(original, at)
    if needs:
        raise ValueError(f"native original material faces are unqualified: {needs[:3]}")
    tables = read(
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    results, panels = [], []
    for query in reports["scalar"]["queries"]:
        result, images = compare_query(
            query,
            roots,
            reports,
            trees,
            original,
            fields,
            at,
            tables,
            provenance["unresolved_originals"],
            args.output,
        )
        results.append(result)
        panels.append(({query["name"] + "-" + k: v for k, v in images.items()}, {}))
    contact(panels, args.output)
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "provenance": provenance,
        "comparisons": results,
        "original_triangles": len(original["indices"]),
        "reference_field_bytes": sum(v.nbytes for v in fields.values()),
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "peak_bytes": peak_rss_bytes(),
        },
        "scope": "Same complete AVAILABLE native geometry plus explicit unrenderable original failures. Current-policy steady-repeat GGX model and finite quadrature; no captured native PBR/shading-normal/shadow/IBL equivalence or whole-world/device/p99 admission. Failed sources keep unknown image and cost.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["cost"]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("scalar", "components", "source", "controls", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    run(parser.parse_args())


if __name__ == "__main__":
    main()
