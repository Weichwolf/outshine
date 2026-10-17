"""Matched complete crown products from the same space and physical leaf inventory."""

import argparse
import json
import resource
import subprocess
import time
from dataclasses import asdict, replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
from botanical_profiles import plant_recipe
from colonization import GrowthBudget, grow
from crown_shoots import CrownShoots
from growth_experiment import validate_plan
from oak_scaffold import OakRecipe
from outshine_ref.vegetation import Shoot
from spatial_crown_experiment import crown_wood, save


def build_one(output, recipe, points, blades, limbs):
    began = time.perf_counter()
    seed = recipe.skeleton(limbs)
    plan = grow(
        points,
        recipe.height_m,
        0.23,
        recipe.step_m,
        2.5,
        0.38,
        budget=GrowthBudget(16384, 128),
        seed=seed,
    )
    validate_plan(plan, recipe.root_radius_m)
    variants = (np.arange(recipe.leaves, dtype=np.uint32) % 8).astype(np.uint8)
    crown = CrownShoots(
        plan["positions"],
        plan["parents"],
        variants,
        recipe.seed,
        length_m=0.35,
        bend_rad=0.4,
        foliage_depth_m=1.1,
    )
    with patch.object(Shoot, "detail", side_effect=AssertionError("premature leaves")):
        summary = crown.summary()
    growth_ready = time.perf_counter()
    assert not crown._groups
    tables = crown.tables(100000)
    rows = {"leaf_" + k: v for k, v in tables.items()}
    placement_ready = time.perf_counter()
    wood = crown_wood(crown, recipe.root_radius_m)
    geometry_ready = time.perf_counter()
    triangles = save(output, rows, wood, blades, crown.identity)
    np.savez_compressed(
        output / "skeleton.npz",
        positions=crown.positions,
        parents=crown.parents,
        seed_positions=seed[0],
        seed_parents=seed[1],
        remaining=plan["remaining"],
    )
    leaf_min = tables["origins"].min(axis=0)
    leaf_max = tables["origins"].max(axis=0)
    result = {
        "identity": crown.identity,
        "summary": summary,
        "source_seed_nodes": len(seed[1]),
        "growth": {
            k: v
            for k, v in plan.items()
            if k not in ("positions", "parents", "remaining")
        },
        "unreached_attractors": len(plan["remaining"]),
        "skeleton_bounds_m": [
            crown.positions.min(axis=0).tolist(),
            crown.positions.max(axis=0).tolist(),
        ],
        "leaf_anchor_bounds_m": [leaf_min.tolist(), leaf_max.tolist()],
        "growth_and_summary_s": growth_ready - began,
        "placement_s": placement_ready - growth_ready,
        "wood_s": geometry_ready - placement_ready,
        "export_s": time.perf_counter() - geometry_ready,
        "instanced_triangles": triangles,
        "wood_triangles": len(wood["faces"]),
        "skeleton_array_bytes": crown.positions.nbytes + crown.parents.nbytes,
        "group_array_bytes": sum(
            a.nbytes for group in crown._groups.values() for a in group.values()
        ),
        "complete_row_array_bytes": sum(a.nbytes for a in rows.values()),
        "wood_array_bytes": sum(a.nbytes for a in wood.values()),
        "known_group_to_whole_copy_bytes": sum(a.nbytes for a in rows.values()),
    }
    return result


def build(output, recipe):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private products must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    points = recipe.growth_space()
    blades = tuple(
        replace(b, length=b.length * 7 / 6, width=b.width * 7 / 6)
        for b in plant_recipe("quercus", recipe.seed).blades
    )
    counts = np.bincount(np.arange(recipe.leaves) % 8, minlength=8)
    area = float(sum(b.area() * count for b, count in zip(blades, counts, strict=True)))
    variants = {}
    for name, limbs in (("recursive", False), ("spatial", True)):
        variants[name] = build_one(output / name, recipe, points, blades, limbs)
        print(json.dumps({"product": name, **variants[name]}), flush=True)
    np.savez_compressed(output / "growth-space.npz", attractors=points)
    metadata = {
        "format": "oak-scaffold-study-1",
        "recipe": asdict(recipe),
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "products": variants,
        "analytic_leaf_area_m2": area,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "presentation_labels": {
            "recursive": "straight-trunk initial state",
            "spatial": "six cubic primary limbs",
        },
        "presentation_camera": {
            "eye": [0, -55, 24],
            "target": [0, 0, 8.5],
            "span_m": 42,
            "separation_m": 9,
            "ground_size_m": 48,
            "resolution": [2560, 1440],
        },
        "scope": "authored mature-size European oak interpretation, same original leaf inventory and growth space, no measured allometry/species/LOD/native acceptance; old closed segment wood and procedural preview materials",
    }
    (output / "comparison.json").write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=682)
    args = parser.parse_args()
    build(args.output, OakRecipe(seed=args.seed))
