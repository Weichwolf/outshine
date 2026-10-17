"""Request cached twigs from both primary populations without rerunning growth."""

import argparse
import hashlib
import json
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import Needle, NeedledPlant, NeedlePattern, Profile
from parent_profile_graph import ParentProfileGraph
from spruce_growth_product import support
from window_portrait_source import source


def replay(product, catalog):
    with (
        patch.object(
            ParentProfileGraph, "instance", side_effect=AssertionError("growth")
        ),
        patch.object(
            ParentProfileGraph, "instances", side_effect=AssertionError("growth")
        ),
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("whole near plant")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("new meshes")),
    ):
        graph, axes, windows, _, proof = source(product, product / "execution", catalog)
        recipe = json.loads((product / "plant-recipe.json").read_text())
        depths = np.diff(axes["path_offsets"]).astype(int) - 2
        families = np.zeros(len(depths), dtype=int)
        active = np.flatnonzero(depths > 0)
        families[active] = axes["path_pairs"][axes["path_offsets"][active] + 2, 0]
        rows = []
        for order in (1, 2, 3):
            for family in (2, 5):
                candidates = np.flatnonzero(
                    (depths == order)
                    & (families == family)
                    & (axes["needle_counts"] <= 1024)
                )
                if not len(candidates):
                    raise ValueError(
                        "one bounded cached twig per family/order required"
                    )
                index = int(max(candidates, key=lambda i: axes["needle_counts"][i]))
                axis = support(axes, index)
                end = int(axes["path_offsets"][index + 1]) - 1
                node_id = int(axes["path_pairs"][end, 0])
                shape = next(n["shape"] for n in graph["nodes"] if n["id"] == node_id)
                profile = Profile(
                    np.array(shape["radius"]["table"]), shape["radius"]["kind"]
                )
                radii = (
                    profile.evaluate(np.array([0.0, 1.0]))
                    * axes["uniform_scales"][index]
                )
                pattern = NeedlePattern.from_recipe(
                    recipe["physical_source"]["patterns"][order]
                )
                count = int(axes["needle_counts"][index])
                if pattern.detail(axis, radii, graph["seed"], count - 1) is not None:
                    raise AssertionError("demand must precede near arrays")
                detail = pattern.detail(axis, radii, graph["seed"], count)
                owned = windows["axis_ids"] == index
                first = int(axes["first_needle_ids"][index])
                starts = windows["first_needle_ids"][owned]
                counts = windows["needle_counts"][owned]
                np.testing.assert_array_equal(starts[1:], starts[:-1] + counts[:-1])
                assert (
                    int(starts[0]) == first
                    and int(starts[-1] + counts[-1]) == first + count
                )
                for values in detail.values():
                    assert np.isfinite(values).all() and not values.flags.writeable
                rows.append(
                    {
                        "order": order,
                        "primary_generator": family,
                        "axis_id": index,
                        "node_id": node_id,
                        "needles": count,
                        "near_array_bytes": sum(a.nbytes for a in detail.values()),
                        "arrays": {
                            k: hashlib.sha256(v.tobytes()).hexdigest()
                            for k, v in detail.items()
                        },
                    }
                )
    return {
        "identity": proof["identity"],
        "requested": rows,
        "scope": "Six real cached twigs, both primary families and all needle orders; no growth/whole near plant/needle mesh. Ledger consistency, not species or LOD approval.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("product", "catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(
        json.dumps(replay(args.product, args.catalog), indent=2) + "\n"
    )
