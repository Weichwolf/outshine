"""Replay only three requested twig details from a complete cached finite plant."""

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
            NeedledPlant, "foliage", side_effect=AssertionError("whole organs")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("needle mesh")),
    ):
        graph, axes, windows, _, proof = source(product, product / "execution", catalog)
        recipe = json.loads((product / "plant-recipe.json").read_text())
        patterns = recipe["physical_source"]["patterns"]
        orders = np.diff(axes["path_offsets"]).astype(np.int64) - 2
        rows = []
        for order in (1, 2, 3):
            available = np.flatnonzero(
                (orders == order) & (axes["needle_counts"] <= 4096)
            )
            if not len(available):
                raise ValueError("bounded requested twigs required at every order")
            index = max(available, key=lambda i: max(support(axes, i).path))
            axis = support(axes, index)
            pattern = NeedlePattern.from_recipe(patterns[order])
            shape = graph["nodes"][order + 1]["shape"]
            profile = Profile(
                np.array(shape["radius"]["table"]), shape["radius"]["kind"]
            )
            radii = (
                profile.evaluate(np.array((0.0, 1.0))) * axes["uniform_scales"][index]
            )
            count = int(axes["needle_counts"][index])
            if pattern.detail(axis, radii, graph["seed"], count - 1) is not None:
                raise AssertionError("request budget must precede detail allocation")
            detail = pattern.detail(axis, radii, graph["seed"], count)
            np.testing.assert_array_equal(detail["ids"], np.arange(count))
            for values in detail.values():
                if not np.isfinite(values).all() or values.flags.writeable:
                    raise AssertionError("finite immutable near arrays required")
            first = int(axes["first_needle_ids"][index])
            owned = windows["axis_ids"] == index
            starts = windows["first_needle_ids"][owned]
            counts = windows["needle_counts"][owned]
            assert int(starts[0]) == first
            assert int(starts[-1]) + int(counts[-1]) == first + count
            np.testing.assert_array_equal(starts[1:], starts[:-1] + counts[:-1])
            rows.append(
                {
                    "order": order,
                    "axis_id": int(index),
                    "path": axis.path,
                    "global_needle_range": [first, first + count],
                    "near_array_bytes": sum(a.nbytes for a in detail.values()),
                    "arrays": {
                        name: {
                            "dtype": a.dtype.str,
                            "shape": a.shape,
                            "sha256": hashlib.sha256(a.tobytes()).hexdigest(),
                        }
                        for name, a in detail.items()
                    },
                }
            )
        if not any(max(r["path"]) >= 64 for r in rows):
            raise ValueError("real graph slots above the legacy limit required")
    return {
        "identity": proof["identity"],
        "requested": rows,
        "scope": "Fresh cache hit, three twig details only; no growth, complete foliage or needle mesh. Ledger ownership, not individual-needle fidelity of statistical windows.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plant-product", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(
        json.dumps(replay(args.plant_product, args.catalog), indent=2) + "\n"
    )
