"""Separate parent shader work and compare fixed aggregated material states."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from building_mass_material_image import evaluate
from building_mass_materials import policy
from building_material_reference import face_materials
from building_parent_shading_cases import counterexamples, plot_counterexamples
from building_parent_shading_work import (
    candidate_face_lobes,
    factor_work,
    projected_work,
)
from building_tree_load import load_cut, read_tree
from building_tree_queries import view
from building_tree_transfer import transfers
from compare_building_brdf import fitted_cut
from compare_building_hierarchy import complete, write_images
from compare_building_regional_hierarchy import rest_price
from compare_building_source_models import contact
from compare_mass_appearance import errors, read
from fit_building_brdf import validate_fit_receipt
from native_building_cell_reference import load_cells
from process_memory import peak_rss_bytes
from triangle_image_reference import projected_bounds


def render_models(original, fields, product, variants, mesh, camera, box, tables, rate):
    images, work = {}, {}
    full = variants["full"]
    images["original"], work["original"] = evaluate(
        original, original, fields, full, product, camera, box, tables, supersample=rate
    )
    images["opaque-full"], work["opaque-full"] = evaluate(
        mesh,
        original,
        fields,
        full,
        product,
        camera,
        box,
        tables,
        parent=True,
        supersample=rate,
    )
    for name, material in variants.items():
        images[name], work[name] = transfers(
            mesh, original, fields, product, material, camera, box, tables, rate
        )
        if (
            name != "full"
            and images[name]["T"].tobytes() != images["full"]["T"].tobytes()
        ):
            raise ValueError(
                "material-only aggregation must retain exact transmittance"
            )
    return images, work


def compare_query(
    root, case, tree, query, original, fields, provenance, fits, at, tables, output
):
    product, full, loading = load_cut(root, case, tree, query, at)
    mesh, _ = complete(original, product)
    variants = {"full": full}
    fit_reads = {}
    for name, budget in (("one", 1), ("two", 2)):
        variants[name], weight_bytes = fitted_cut(
            root, fits / str(budget), case, query, full, budget
        )
        names = [
            case["node_files"][str(n)]
            for n in query["nodes"]
            if str(n) in case["node_files"]
        ]
        fit_reads[name] = {
            "dense_fitted_weight_bytes": weight_bytes,
            "raw_source_container_bytes_read_for_binding": sum(
                (root / n).stat().st_size for n in names
            ),
            "sidecar_container_bytes_accessed": sum(
                (fits / str(budget) / n).stat().st_size for n in names
            ),
        }
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
    pixels = int(np.prod(box[1] - box[0]))
    if np.any(box[1] <= box[0]) or pixels * 4 > 1048576:
        raise ValueError("bounded visible shared/refined ROI required")
    rate = 16
    while pixels * (2 * rate) ** 2 > 1048576 and rate > 1:
        rate //= 2
    images, work = render_models(
        original, fields, product, variants, mesh, camera, box, tables, rate
    )
    refined, _ = render_models(
        original, fields, product, variants, mesh, camera, box, tables, 2 * rate
    )
    model = {
        "original": projected_work(
            original, camera, np.count_nonzero(fields["weights"], axis=-1).max(axis=1)
        )
    }
    for name, material in variants.items():
        model[name] = projected_work(
            mesh,
            camera,
            candidate_face_lobes(original, fields, product, material, mesh),
        )
    write_images(output, query["name"], images, {}, camera, box)
    np.savez(
        output / (query["name"] + "-refined.npz"),
        **{n + "__" + k: v for n, img in refined.items() for k, v in img.items()},
    )
    return {
        "query": query,
        "roi": box.tolist(),
        "sample_rate": rate,
        "original_rest": rest_price(
            original, product, provenance["unresolved_originals"], fields=fields
        ),
        "loading": loading,
        "fit_reads": fit_reads,
        "work": work,
        "work_factors": factor_work(
            work["original"], work["opaque-full"], work["full"]
        ),
        "errors": {
            n: errors(img, images["original"])
            for n, img in images.items()
            if n != "original"
        },
        "vs_full": {n: errors(images[n], images["full"]) for n in ("one", "two")},
        "refined_errors": {
            n: errors(img, refined["original"])
            for n, img in refined.items()
            if n != "original"
        },
        "quadrature_change": {n: errors(img, images[n]) for n, img in refined.items()},
        "projected_primitive_model": model,
        "parent_array_bytes": {
            k: product[k].nbytes
            for k in ("positions", "indices", "triangle_groups", "group_coverage")
        },
        "material_array_bytes": {
            n: {
                k: m[k].nbytes
                for k in ("albedo", "roughness_weights", "roughness_palette")
            }
            for n, m in variants.items()
        },
    }, images


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    cpu, wall = time.process_time(), time.perf_counter()
    tables_path = (
        Path(__file__).resolve().parents[2]
        / "outshine/data/render/material-filter/energy-tables.npz"
    )
    for budget in (1, 2):
        validate_fit_receipt(args.fits / str(budget), budget, tables_path)
    case = json.loads((args.input / "report.json").read_text())
    tree, at, tables = read_tree(args.input), policy(), read(tables_path)
    original, provenance = load_cells(
        args.controls, args.source, set(case["cells"]), tree
    )
    fields, needs = face_materials(original, at)
    if needs:
        raise ValueError("all available native material faces must be qualified")
    results, panels = [], []
    for query in case["queries"]:
        result, images = compare_query(
            args.input,
            case,
            tree,
            query,
            original,
            fields,
            provenance,
            args.fits,
            at,
            tables,
            args.output,
        )
        results.append(result)
        panels.append(({query["name"] + "-" + k: v for k, v in images.items()}, {}))
    contact(panels, args.output)
    cases = counterexamples(args.input, args.fits, tables)
    (args.output / "counterexamples.json").write_text(
        json.dumps(cases, indent=2) + "\n"
    )
    plot_counterexamples(cases, args.output / "counterexamples.png")
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "provenance": provenance,
        "comparisons": results,
        "counterexamples": "counterexamples.json",
        "cost": {
            "cpu_ms": (time.process_time() - cpu) * 1000,
            "wall_ms": (time.perf_counter() - wall) * 1000,
            "peak_bytes": peak_rss_bytes(),
        },
        "native_cost_model": "Measured submitted/reference bytes and counts; pre-depth projected triangle-box pixel/quad slots; reference GGX/layer work. Device costs for setup, coverage, material, shading, sorting/composition and memory are unmeasured. Neither divide SS16 work by256 nor convert CPU counters to GPU time; alpha/order overhead remains a separate native requirement.",
        "scope": "Same four declared views/light and complete AVAILABLE native opponent/original rest; two failed sources remain required with unknown geometry/image/cost. Only parent roughness weights vary. Fixed finite angular fits and16/32 radiance samples, no certified global material/normal/shadow/transition/device/world/p99 bound or new native format/default.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["cost"]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("input", "fits", "source", "controls", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    run(parser.parse_args())


if __name__ == "__main__":
    main()
