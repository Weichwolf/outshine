"""Prepare existing attributed solids, then independent complete-cell PBR opponents."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from attributed_prisms import compile_product, load_product
from compact_attributed_prisms import compact
from compare_far_height_profiles import comparison_views
from full_view_reference import load, views
from prepared_surface_recipes import load_recipes
from prism_diagnostic_mesh import represented_mesh, write_scene
from prism_field_storage_control import control


def camera_cases(source, document, context):
    poses = views(document, context)
    base, centre, right, up = comparison_views(source, document, context)
    names = ["static-020", "moving-020", "moving-021", "moving-023", "static-025"]
    chosen = {name: poses[name] for name in names}
    chosen["near-control"] = base["near-control"]
    basis = np.stack((right, np.cross(up, right), up))
    eye = (base["static-020"][0] - centre) @ basis.T + [0, 0, 1500]
    forward = -eye / np.linalg.norm(eye)
    x = np.cross(forward, [0, 0, 1])
    x /= np.linalg.norm(x)
    camera = np.stack((x, np.cross(x, forward), forward))
    chosen["roof-control"] = (
        eye @ basis + centre,
        camera @ basis,
        *base["static-020"][2:],
    )
    return chosen, basis, centre


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    source, document, context, receipt = load(args.input)
    recipes, recipe_receipt = load_recipes(args.input, document)
    before = time.perf_counter()
    arrays, reasons = compile_product(document, source, recipes)
    compile_seconds = time.perf_counter() - before
    dense = arrays
    arrays = compact(dense)
    np.savez_compressed(args.output / "dense-product.npz", **dense)
    np.savez_compressed(args.output / "product.npz", **arrays)
    exact, exact_report = control(source)
    np.savez_compressed(args.output / "exact-control.npz", **exact)
    product = load_product(args.output / "product.npz")
    started = time.perf_counter()
    mesh, residual = represented_mesh(product)
    mesh_seconds = time.perf_counter() - started
    original = {k: source[k] for k in ["positions", "indices", "colour"]}
    original["shading_normal"] = source["vertex_normal"]
    original["source_id"] = source["binding"][:, 1]
    if set(mesh["source_id"]) != set(original["source_id"]):
        raise ValueError("Complete original source partition changed")
    named, basis, centre = camera_cases(source, document, context)
    local = (source["positions"].astype(float) - centre) @ basis.T
    ground = {
        "z": float(local[:, 2].min() - 1),
        "span": float(np.ptp(local[:, :2], axis=0).max() * 4),
    }
    producer = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    for name, view in named.items():
        for strategy, data in [("original", original), ("prisms", mesh)]:
            write_scene(
                args.output / name / strategy,
                data,
                view,
                basis,
                centre,
                ground,
                producer,
            )
    report = {
        "producer": producer,
        "input_receipt": receipt,
        "recipe_receipt": recipe_receipt,
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "product_arrays": {key: array.nbytes for key, array in arrays.items()},
        "dense_product_arrays": {key: array.nbytes for key, array in dense.items()},
        "exact_control": exact_report,
        "compile_seconds": compile_seconds,
        "diagnostic_mesh_seconds": mesh_seconds,
        "diagnostic_mesh_arrays": {key: array.nbytes for key, array in mesh.items()},
        "original_triangles": len(source["indices"]),
        "diagnostic_triangles": len(mesh["indices"]),
        "represented_sources": len(set(product[0].binding[:, 0])),
        "original_fallback_reasons": reasons,
        "maximum_diagnostic_plane_residual_m": residual,
        "poses": list(named),
        "scope": "Existing #897 product with complete original fallback, unchanged compiler/clip/BVH and explicit code/Gable. Diagnostic triangles are private verification output, not proposed native assets or a replacement product. Neutral PBR only; native procedural material/opacity/shadow/filter/device contracts remain open.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: v
                for k, v in report.items()
                if k not in ["input_receipt", "recipe_receipt"]
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
