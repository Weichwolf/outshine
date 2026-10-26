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
from building_parent_shading_work import candidate_face_lobes, projected_work
from building_tree_load import load_cut, read_tree
from building_tree_queries import view
from building_tree_transfer import transfers
from compare_building_brdf import fitted_cut
from compare_building_hierarchy import complete, continuous, write_images
from compare_building_source_models import contact
from compare_directed_groups import retained_subset
from compare_mass_appearance import errors, read
from fit_building_brdf import validate_fit_receipt
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


def response_models(products, roots, reports, query, fit_roots):
    models, reads = {}, {}
    for name, (_, material, _) in products.items():
        models[name] = (name, material)
        if fit_roots is None:
            continue
        fitted, size = fitted_cut(
            roots[name], fit_roots[name], reports[name], query, material, 2
        )
        models[name + "-two"] = (name, fitted)
        files = [
            reports[name]["node_files"][str(n)]
            for n in query["nodes"]
            if str(n) in reports[name]["node_files"]
        ]
        reads[name] = {
            "dense_fitted_weight_bytes": size,
            "raw_source_container_bytes_read_for_binding": sum(
                (roots[name] / n).stat().st_size for n in files
            ),
            "sidecar_container_bytes_accessed": sum(
                (fit_roots[name] / n).stat().st_size for n in files
            ),
        }
    return models, reads


def render_models(
    original, fields, products, models, meshes, camera, box, tables, rate, *, opaque
):
    product, material, _ = products["scalar"]
    images, work = {}, {}
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
    for name, (geometry, material) in models.items():
        product = products[geometry][0]
        if opaque:
            images[name + "-opaque"], work[name + "-opaque"] = evaluate(
                meshes[geometry],
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
            meshes[geometry],
            original,
            fields,
            product,
            material,
            camera,
            box,
            tables,
            rate,
        )
        if (
            name != geometry
            and images[name]["T"].tobytes() != images[geometry]["T"].tobytes()
        ):
            raise ValueError("material-only fit must retain exact transmittance")
    return images, work


def candidate_costs(models, products, meshes, original, fields, camera):
    primitives = {
        "original": projected_work(
            original, camera, np.count_nonzero(fields["weights"], axis=-1).max(axis=1)
        )
    }
    for name, (geometry, material) in models.items():
        product, mesh = products[geometry][0], meshes[geometry]
        primitives[name] = projected_work(
            mesh,
            camera,
            candidate_face_lobes(original, fields, product, material, mesh),
        )
    return {
        "reference_material_array_bytes": {
            name: {k: v.nbytes for k, v in material.items()}
            for name, (_, material) in models.items()
        },
        "parent_geometry_arrays": {
            name: {
                k: {
                    "dtype": product[k].dtype.str,
                    "shape": product[k].shape,
                    "bytes": product[k].nbytes,
                }
                for k in (
                    "positions",
                    "indices",
                    "triangle_groups",
                    "group_coverage",
                    "needs_sources",
                )
            }
            for name, (product, _, _) in products.items()
        },
        "projected_primitive_model": primitives,
    }


def compare_query(
    query,
    roots,
    reports,
    trees,
    original,
    fields,
    at,
    tables,
    unresolved,
    output,
    *,
    fit_roots=None,
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
    models, fit_reads = response_models(products, roots, reports, query, fit_roots)
    areas = {"original": continuous(original, camera, box)[0]}
    for name, mesh in meshes.items():
        areas[name] = continuous(mesh, camera, box)[0]
    images, work = render_models(
        original,
        fields,
        products,
        models,
        meshes,
        camera,
        box,
        tables,
        rate,
        opaque=fit_roots is None,
    )
    refined, _ = render_models(
        original,
        fields,
        products,
        models,
        meshes,
        camera,
        box,
        tables,
        2 * rate,
        opaque=False,
    )
    for name, (_, _, loading) in products.items():
        prices[name]["loading"] = loading
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
    if fit_roots is not None:
        report["fitted_state_reads"] = fit_reads
        report.update(
            candidate_costs(models, products, meshes, original, fields, camera)
        )
        report["component_two_vs_scalar_two"] = errors(
            images["components-two"], images["scalar-two"]
        )
    return report, images


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    cpu, wall = time.process_time(), time.perf_counter()
    roots = {"scalar": args.scalar, "components": args.components}
    reports = {k: json.loads((p / "report.json").read_text()) for k, p in roots.items()}
    fit_roots = None
    if (args.fits_scalar is None) != (args.fits_components is None):
        raise ValueError("both fixed two-lobe fit roots are required together")
    if args.fits_scalar is not None:
        fit_roots = {"scalar": args.fits_scalar, "components": args.fits_components}
        tables_path = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/material-filter/energy-tables.npz"
        )
        for root in fit_roots.values():
            validate_fit_receipt(root, 2, tables_path)
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
            fit_roots=fit_roots,
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
    if fit_roots is not None:
        report["whole_tree_reference_array_bytes"] = {
            name: sum(v.nbytes for v in tree.values()) for name, tree in trees.items()
        }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["cost"]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("scalar", "components", "source", "controls", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--fits-scalar", type=Path)
    parser.add_argument("--fits-components", type=Path)
    run(parser.parse_args())


if __name__ == "__main__":
    main()
