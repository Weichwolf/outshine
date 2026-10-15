"""Measure mature length/count alternatives without axes or individual needles."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from mature_density import MAX_AXES, needle_areas
from outshine_ref.vegetation import DistancePattern, NeedledPlant, Profile

MODES = ("absolute-fixed", "relative-fixed", "relative-distance")


def length_orders(wood, mode):
    if mode not in MODES:
        raise ValueError("implemented parent-growth mode required")
    orders = [np.array([wood.levels[0].length_at(0)])]
    for depth, level in enumerate(wood.levels[1:], 1):
        previous = orders[-1]
        if depth == 1 or mode == "absolute-fixed":
            lengths = np.tile(
                [level.length_at(slot) for slot in range(level.branches)],
                len(previous),
            )
        elif mode == "relative-fixed":
            local = np.array([level.length_at(slot) for slot in range(level.branches)])
            lengths = (
                previous[:, None] * local / wood.levels[depth - 1].length
            ).ravel()
        else:
            if (
                level.branches < 2
                or level.length_shape != "taper"
                or level.first == level.last
            ):
                raise ValueError(
                    "distance probe needs a nonempty tapered branch interval"
                )
            parent_length = wood.levels[depth - 1].length
            spacing = parent_length * (level.last - level.first) / (level.branches - 1)
            pattern = DistancePattern(
                spacing=Profile(np.full(4, spacing)), limit_groups=level.branches
            )
            table = pattern.tables(
                previous, level.first, level.last, max_sites=MAX_AXES
            )
            if table is None:
                raise ValueError("physical child placement exceeds the axis budget")
            parameters = np.concatenate([rows[:, 0] for rows in table])
            parents = np.repeat(np.arange(len(previous)), [len(rows) for rows in table])
            fraction = (parameters - level.first) / (level.last - level.first)
            shape = 1 + (level.tip_scale - 1) * fraction
            lengths = previous[parents] * level.length / parent_length * shape
        if sum(len(order) for order in orders) + len(lengths) > MAX_AXES:
            raise ValueError("complete axis count exceeds the existing mature budget")
        orders.append(lengths)
    return orders


def probe(recipe, species):
    plant = NeedledPlant.from_recipe(json.loads(recipe.read_text()))
    areas, hashes = needle_areas(species)
    rows = []
    for mode in MODES:
        lengths = length_orders(plant.wood, mode)
        counts = [
            sum(pattern.count(float(length)) for length in order) if pattern else 0
            for order, pattern in zip(lengths, plant.patterns, strict=True)
        ]
        total, first = 0.0, 0
        for count, pattern in zip(counts, plant.patterns, strict=True):
            cycles, remainder = divmod(count, len(areas))
            variants = np.full(len(areas), cycles, dtype=np.int64)
            for index in range(remainder):
                variants[(first + plant.wood.seed + index) % len(areas)] += 1
            variance = pattern.scale_variation**2 / 3 if pattern else 0
            total += float(variants @ areas) * (1 + variance)
            first += count
        row = {
            "mode": mode,
            "axis_counts": [len(order) for order in lengths],
            "needle_counts": counts,
            "expected_area_m2": total,
            "length_min_median_max_m": [
                np.quantile(order, (0, 0.5, 1)).tolist() for order in lengths
            ],
        }
        if mode == "absolute-fixed":
            expected = plant.area_summary(areas)
            assert tuple(counts) == plant.needle_counts
            np.testing.assert_allclose(total, expected["expected_area_m2"], rtol=1e-14)
        rows.append(row)
    assert not plant.wood._axes
    return {
        "source_identity": plant.identity,
        "recipe": str(recipe),
        "recipe_sha256": hashlib.sha256(recipe.read_bytes()).hexdigest(),
        "physical_area_inputs_sha256": hashes,
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", __file__], text=True
            ).strip()
        ),
        "implementation_sha256": hashlib.sha256(
            Path(__file__).read_bytes()
        ).hexdigest(),
        "scope": "analytic count/length alternatives; baseline checked against installed area kernel; relative modes are proposed layouts, not production IDs or botanical/visual approval; no axes or organs created",
        "alternatives": rows,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", type=Path, required=True)
    parser.add_argument(
        "--species", choices=("picea-abies", "pinus-sylvestris"), required=True
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = probe(args.recipe, args.species)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["alternatives"]))
