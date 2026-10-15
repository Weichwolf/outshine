"""Measure mature length/count alternatives without axes or individual needles."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from mature_density import MAX_AXES, WINDOW_SETTINGS, needle_areas
from outshine_ref.vegetation import DistancePattern, NeedledPlant, Profile
from outshine_ref.vegetation.needle_windows import capacities

MODES = ("absolute-fixed", "relative-fixed", "relative-distance")


class AxisBudgetExceeded(ValueError):
    pass


def length_orders(wood, mode):
    if mode not in MODES:
        raise ValueError("implemented parent-growth mode required")
    orders = [np.array([wood.levels[0].length_at(0)])]
    for depth, level in enumerate(wood.levels[1:], 1):
        previous = orders[-1]
        remaining = MAX_AXES - sum(len(order) for order in orders)
        fixed = depth == 1 or mode != "relative-distance" or level.branches == 1
        if fixed and len(previous) * level.branches > remaining:
            raise AxisBudgetExceeded("complete axis count exceeds the mature budget")
        if depth == 1 or mode == "absolute-fixed":
            lengths = np.tile(
                [level.length_at(slot) for slot in range(level.branches)],
                len(previous),
            )
        elif mode == "relative-fixed" or level.branches == 1:
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
                previous, level.first, level.last, max_sites=remaining
            )
            if table is None:
                raise AxisBudgetExceeded(
                    "physical child placement exceeds the axis budget"
                )
            parameters = np.concatenate([rows[:, 0] for rows in table])
            parents = np.repeat(np.arange(len(previous)), [len(rows) for rows in table])
            fraction = (parameters - level.first) / (level.last - level.first)
            shape = 1 + (level.tip_scale - 1) * fraction
            lengths = previous[parents] * level.length / parent_length * shape
        if sum(len(order) for order in orders) + len(lengths) > MAX_AXES:
            raise AxisBudgetExceeded("complete axis count exceeds the mature budget")
        orders.append(lengths)
    return orders


def expected_area(counts, patterns, seed, areas):
    total, first = 0.0, 0
    for count, pattern in zip(counts, patterns, strict=True):
        cycles, remainder = divmod(count, len(areas))
        variants = np.full(len(areas), cycles, dtype=np.int64)
        for index in range(remainder):
            variants[(first + seed + index) % len(areas)] += 1
        variance = pattern.scale_variation**2 / 3 if pattern else 0
        total += float(variants @ areas) * (1 + variance)
        first += count
    return total


def measure(plant, areas, mode):
    lengths = length_orders(plant.wood, mode)
    counts, windows = [], 0
    groups = capacities(plant, **WINDOW_SETTINGS)
    for order, pattern, capacity in zip(lengths, plant.patterns, groups, strict=True):
        local = [pattern.count(float(length)) for length in order] if pattern else []
        counts.append(sum(local))
        if local:
            size = capacity * pattern.group_size
            windows += sum((count + size - 1) // size for count in local)
    total = expected_area(counts, plant.patterns, plant.wood.seed, areas)
    return {
        "mode": mode,
        "status": "measured",
        "axis_counts": [len(order) for order in lengths],
        "needle_counts": counts,
        "windows": windows,
        "expected_area_m2": total,
        "length_min_median_max_m": [
            np.quantile(order, (0, 0.5, 1)).tolist() for order in lengths
        ],
    }


def probe(recipe, species, needle_source):
    plant = NeedledPlant.from_recipe(json.loads(recipe.read_text()))
    areas, hashes = needle_areas(species, needle_source)
    rows = []
    for mode in MODES:
        try:
            row = measure(plant, areas, mode)
        except AxisBudgetExceeded as error:
            rows.append(
                {"mode": mode, "status": "budget-refused", "reason": str(error)}
            )
            continue
        if mode == "absolute-fixed":
            expected = plant.area_summary(areas)
            assert tuple(row["needle_counts"]) == plant.needle_counts
            assert row["windows"] == plant.window_summary(**WINDOW_SETTINGS)["windows"]
            np.testing.assert_allclose(
                row["expected_area_m2"], expected["expected_area_m2"], rtol=1e-14
            )
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
        "absolute_baseline_checked": rows[0]["status"] == "measured",
        "scope": "analytic count/length alternatives; each feasible absolute baseline checked against installed count/area/window kernels; relative modes are proposed layouts, not production IDs or botanical/visual approval; no axes or organs created",
        "alternatives": rows,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", type=Path, required=True)
    parser.add_argument(
        "--species", choices=("picea-abies", "pinus-sylvestris"), required=True
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--needle-source", type=Path, required=True)
    args = parser.parse_args()
    result = probe(args.recipe, args.species, args.needle_source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result["alternatives"]))
