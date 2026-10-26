"""Fresh finished-mesh cache hit, then independent one-organ recipe regeneration."""

import argparse
import json
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import Blade, BladeCutout, PlantPlan
from tree_entry import compare_rows, load, reference


def run(inputs, output):
    start = time.perf_counter()
    with (
        patch.object(BladeCutout, "mesh", side_effect=AssertionError("mesh on hit")),
        patch.object(Blade, "evaluate", side_effect=AssertionError("mapping on hit")),
        patch.object(
            PlantPlan, "tables", side_effect=AssertionError("placement on hit")
        ),
    ):
        metadata, products = load(inputs)
        _, expected = reference()
        compare_rows(products["tables.npz"], expected)
        payload = sum(a.nbytes for record in products.values() for a in record.values())
        count = len(products["wood.npz"]["faces"])
        for variant in range(8):
            mesh = products[f"blade-{variant}.npz"]
            count += int((expected["leaf_variants"] == variant).sum()) * len(
                mesh["faces"]
            )
        assert count == metadata["instanced_triangles"]
        mesh_bytes = payload - sum(a.nbytes for a in expected.values())
        assert mesh_bytes == metadata["canonical_mesh_payload_bytes"]
    ready = time.perf_counter()
    recipe = json.loads((inputs / "cutout-recipes.json").read_text())[0]
    cutout = BladeCutout.from_recipe(recipe)
    regenerated = cutout.mesh(64, 3)
    for name, values in regenerated.items():
        np.testing.assert_array_equal(values, products["blade-0.npz"][name])
    result = {
        "morphology_identity": metadata["morphology_identity"],
        "source_plan_identity": metadata["source_plan_identity"],
        "ready_product_read_check_s": ready - start,
        "one_organ_replay_s": time.perf_counter() - ready,
        "loaded_array_bytes": payload,
        "instanced_triangles": count,
        "checks": "passed",
        "scope": "Cache hit performs no placement, blade mapping or meshing. Separate recipe replay regenerates one requested organ; no native/frame/LOD claim.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.output)
