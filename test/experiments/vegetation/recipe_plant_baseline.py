"""Compare all actual existing cached whole-pine rendering inputs, no new oracle."""

import json

import numpy as np
from outshine_ref.vegetation import NeedledPlant
from outshine_ref.vegetation.axis_surface import axis_mesh
from recipe_graph_data import plain


def equal_arrays(path, expected):
    with np.load(path, allow_pickle=False) as original:
        if set(original.files) != set(expected):
            raise ValueError("Actual prior render buffer fields changed")
        for key, a in expected.items():
            if original[key].dtype != a.dtype:
                raise ValueError("Actual prior render buffer dtype changed")
            np.testing.assert_array_equal(original[key], a)
    return len(expected)


def compare(inputs, detail):
    recipe = plain(detail.recipe)
    if json.loads((inputs / "whole-plan.json").read_text()) != recipe:
        raise ValueError("Actual prior whole-plant recipe changed")
    count = equal_arrays(inputs / "whole-tables.npz", detail.tables)
    metadata = json.loads((inputs / "whole.json").read_text())
    plan = NeedledPlant.from_recipe(recipe)
    tables = detail.tables
    for product in metadata["products"]:
        name = product["mesh"]
        if "-needle-" in name:
            i = int(name.rsplit("-", 1)[-1].split(".")[0])
            mesh = detail.needles[i]
            mask = tables["foliage_variants"] == i
            poses = {
                key: tables["foliage_" + key][mask]
                for key in ("ids", "origins", "frames", "scales")
            }
        elif "-short-" in name:
            depth = int(name.rsplit("-", 1)[-1].split(".")[0])
            pattern = plan.patterns[depth]
            start = sum(plan.wood.axis_counts[:depth])
            ids = tables["foliage_short_axis_ids"]
            mask = (ids >= start) & (ids < start + plan.wood.axis_counts[depth])
            mesh = axis_mesh(
                pattern.short_length, 0, 0.00065, 0.00065 * 0.7, 2, 6, bark=True
            )
            poses = {
                key: tables["foliage_short_" + key][mask]
                for key in ("ids", "origins", "frames")
            }
            poses["scales"] = np.ones(int(mask.sum()))
        else:
            i = int(name.rsplit("-", 1)[-1].split(".")[0])
            depth = int(np.count_nonzero(tables["axis_paths"][i] >= 0))
            mesh = axis_mesh(
                tables["axis_lengths"][i],
                tables["axis_bends"][i],
                *tables["axis_radii"][i],
                16 if depth == 0 else 8,
                12 if depth == 0 else 8,
                bark=True,
            )
            poses = {
                key: tables["axis_" + key][i : i + 1]
                for key in ("ids", "origins", "frames")
            }
            poses["scales"] = np.ones(1)
        assert product["instances"] == len(poses["ids"])
        count += equal_arrays(inputs / name, mesh)
        count += equal_arrays(inputs / product["placements"], poses)
    return {
        "render_products": len(metadata["products"]),
        "exact_arrays_with_dtype": count,
        "scope": "All prior cached wood/short-shoot/needle geometry and poses exact. Existing scene/shader/material files consumed unchanged; no new image improvement.",
    }
