"""Attribute complete cached physical needle area to actual primary attachments."""

import argparse
import json
from pathlib import Path

import numpy as np
from parent_profile_graph import ParentProfileGraph
from spruce_growth_probe import cached_areas
from window_portrait_source import source


def area_distribution(product, catalog, needles):
    recipe, axes, _, _, evidence = source(product, product / "execution", catalog)
    physical = json.loads((product / "plant-recipe.json").read_text())[
        "physical_source"
    ]
    areas, hashes = cached_areas(needles)
    active = np.flatnonzero(axes["needle_counts"])
    count = axes["needle_counts"][active]
    first = axes["first_needle_ids"][active]
    cycles, remainder = np.divmod(count, len(areas))
    variants = (
        first[:, None] + recipe["seed"] + np.arange(len(areas), dtype=np.uint64)
    ) % len(areas)
    partial = (np.arange(len(areas))[None] < remainder[:, None]) * areas[variants]
    per_axis = cycles * areas.sum() + partial.sum(axis=1)
    orders = np.diff(axes["path_offsets"])[active].astype(np.int64) - 2
    factors = np.array(
        [
            1 if p is None else 1 + p["scale_variation"] ** 2 / 3
            for p in physical["patterns"]
        ]
    )
    per_axis *= factors[orders]
    graph = ParentProfileGraph.from_recipe(recipe)
    stem = graph.instance(((0, 0), (1, 0)))
    sites = graph.site_table(2, stem)[:, 0]
    parents = axes["path_pairs"][axes["path_offsets"][active] + 2]
    if np.any(parents[:, 0] != 2) or np.any(parents[:, 1] >= len(sites)):
        raise ValueError("actual four-order primary owner paths required")
    primary = parents[:, 1]
    upper = sites[primary] >= 0.75
    executed = json.loads((product / "execution/execution.json").read_text())
    total = float(per_axis.sum())
    if abs(total - executed["expected_area_m2"]) > 1e-8:
        raise AssertionError("complete per-axis variant/area ledger must reconcile")
    return {
        "identity": evidence["identity"],
        "expected_area_m2": total,
        "upper_stem_quarter_primary_axes": int((sites >= 0.75).sum()),
        "upper_stem_quarter_owned_area_m2": float(per_axis[upper].sum()),
        "upper_stem_quarter_owned_needles": int(count[upper].sum()),
        "area_inputs_sha256": hashes,
        "scope": "Half cached closed needle surface, expected scale squared, deterministic global variant cycles; owner primary attaches at stem parameter>=0.75. Not world-height binning, projected coverage, observed LAI or individual-needle expansion.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("plant-product", "catalog", "needles", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    result = area_distribution(args.plant_product, args.catalog, args.needles)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)
