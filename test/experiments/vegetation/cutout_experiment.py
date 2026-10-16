"""Replace only cached organ prototypes on unchanged fixed tree attachment rows."""

import argparse
import hashlib
import json
import time
from dataclasses import replace
from pathlib import Path

import numpy as np
from cutout_shapes import contour
from outshine_ref.vegetation import BladeCutout, PlantPlan
from tree_entry import IDENTITY, compare_rows, load, reference


def build(inputs, output):
    start = time.perf_counter()
    metadata, products = load(inputs)
    fixed, expected = reference()
    compare_rows(products["tables.npz"], expected)
    plant = PlantPlan.from_recipe(fixed["recipe"])
    assert plant.identity == IDENTITY
    output.mkdir(parents=True, exist_ok=False)
    for path in inputs.iterdir():
        if (
            path.is_file()
            and not path.name.startswith("blade-")
            and path.name != "entry.json"
        ):
            (output / path.name).symlink_to(path.resolve())
    recipes, records, total = [], [], len(products["wood.npz"]["faces"])
    for variant, original in enumerate(plant.blades):
        carrier = replace(original, lobes=0, lobe_depth=0)
        cutout = BladeCutout(carrier, contour(carrier, 71 + variant))
        mesh = cutout.mesh(64, 3)
        if mesh is None:
            raise ValueError("complete leaf prototype did not fit explicit capacity")
        mesh["corner_uv"] = np.c_[
            (mesh["parameters"][:, 1] + 1) / 2, mesh["parameters"][:, 0]
        ][mesh["faces"].ravel()]
        mesh["surface"] = np.full(len(mesh["faces"]), 6, np.uint8)
        np.savez_compressed(output / f"blade-{variant}.npz", **mesh)
        count = int((expected["leaf_variants"] == variant).sum())
        total += count * len(mesh["faces"])
        recipes.append(cutout.recipe())
        p = mesh["vertices"][mesh["faces"]]
        area = (
            np.linalg.norm(np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1).sum()
            / 2
        )
        records.append(
            {
                "variant": variant,
                "identity": cutout.identity,
                "triangles": len(mesh["faces"]),
                "payload_bytes": sum(a.nbytes for a in mesh.values()),
                "mesh_area_m2": float(area),
                "instances": count,
            }
        )
    physical_id = hashlib.sha256(
        json.dumps(recipes, sort_keys=True).encode()
    ).hexdigest()
    metadata.update(
        {
            "format": "tree-cutout-products-1",
            "source_plan_identity": IDENTITY,
            "morphology_identity": physical_id,
            "instanced_triangles": total,
            "canonical_mesh_payload_bytes": sum(
                a.nbytes for a in products["wood.npz"].values()
            )
            + sum(r["payload_bytes"] for r in records),
            "build_s": time.perf_counter() - start,
            "scope": "Same 64 axes/571 stored leaf attachments; own asymmetric cutouts on unlobed carriers. Changed morphology/area, not old PlantPlan coverage. Shared old pigment/ridge preview, no botanical/material/LOD admission.",
        }
    )
    for name in ("plan_and_reference_s", "geometry_s"):
        metadata.pop(name, None)
    (output / "entry.json").write_text(json.dumps(metadata, indent=2) + "\n")
    (output / "cutout-recipes.json").write_text(json.dumps(recipes, indent=2) + "\n")
    report = {
        "source_plan": IDENTITY,
        "morphology": physical_id,
        "leaves": records,
        "instanced_triangles": total,
        "build_s": time.perf_counter() - start,
    }
    (output / "cutout-build.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.inputs, args.output)
