"""Existing profile-only upper-crown controls, bounded before full spruce growth."""

import argparse
import copy
import json
from pathlib import Path

import numpy as np
from outshine_ref.vegetation import Profile
from parent_profile_graph import ParentProfileGraph
from spruce_growth_probe import cached_areas, internode_profile, measure


def profiles(original, upper_angle, *, last=None, terminal_fraction=None, exponent=1.0):
    recipe = copy.deepcopy(original)
    primary = recipe["nodes"][2]
    placement = primary["placement"]
    first, old_last = placement["first"], placement["last"]
    last = old_last if last is None else last
    if (
        not np.isfinite([upper_angle, last, exponent]).all()
        or not 0.75 <= upper_angle <= 1.3
        or not old_last <= last <= 0.995
        or not 0.7 <= exponent <= 1.1
    ):
        raise ValueError("declared bounded upper-crown profile controls required")
    inclination = next(p for key, p in recipe["inclinations"] if key == 2)
    lower = float(
        Profile(np.array(inclination["table"]), inclination["kind"]).evaluate(first)
    )
    table = lower + (upper_angle - lower) * (np.linspace(0, 1, 4) - first) / (
        last - first
    )
    inclination.update(kind="custom", table=table.tolist())
    if last != old_last or terminal_fraction is not None:
        if terminal_fraction is None or not 0.01 <= terminal_fraction <= 0.09:
            raise ValueError("explicit physical terminal length fraction required")
        old = placement["scale"]
        base = float(Profile(np.array(old["table"]), old["kind"]).evaluate(first))
        t = np.linspace(0, 1, 2001)
        remaining = np.clip((last - t) / (last - first), 0, 1)
        table = base * (
            terminal_fraction + (1 - terminal_fraction) * remaining**exponent
        )
        placement["scale"] = {"kind": "custom", "table": table.tolist()}
        placement["last"] = last
        pattern = placement["pattern"]
        pattern["spacing"]["table"] = internode_profile(
            recipe["nodes"][1]["shape"]["length"],
            first,
            last,
            pattern["limit_groups"],
            recipe["seed"],
            0.35,
        )
    return recipe


def variants(original):
    return [
        ("control", copy.deepcopy(original), {}),
        *[
            (
                f"angle-{angle}",
                profiles(original, angle),
                {"upper_inclination_rad": angle},
            )
            for angle in (0.85, 1.05, 1.25)
        ],
        *[
            (
                f"leader-{power}",
                profiles(
                    original, 1.05, last=0.995, terminal_fraction=0.02, exponent=power
                ),
                {
                    "upper_inclination_rad": 1.05,
                    "last": 0.995,
                    "terminal_length_fraction": 0.02,
                    "length_exponent": power,
                },
            )
            for power in (1.0, 0.9, 0.8, 0.7)
        ],
    ]


def primary_summary(recipe):
    graph = ParentProfileGraph.from_recipe(recipe)
    stem = graph.instance(((0, 0), (1, 0)))
    primary = graph.instances(2, 182)
    if primary is None or len(primary) != 180:
        raise ValueError("same complete180primary branches required")
    sites = graph.site_table(2, stem)[:, 0]
    upper = sites >= 0.75
    directions = np.array([p.frame[:, 1] for p in primary])
    # The trunk tilts slightly. Measure inclination in its physical frame.
    cosines = directions @ stem.frame[:, 1]
    return {
        "primary_axes": len(primary),
        "prepared_poses": len(graph._poses),
        "upper_quarter_primary_axes": int(upper.sum()),
        "upper_quarter_mean_length_m": float(
            np.mean(
                [p.length for p, active in zip(primary, upper, strict=True) if active]
            )
        ),
        "upper_quarter_mean_inclination_rad": float(
            np.arccos(np.clip(cosines[upper], -1, 1)).mean()
        ),
        "last_primary_parameter": float(sites.max()),
        "unoccupied_leader_arc_length_m": float((1 - sites.max()) * stem.length),
    }


def probe(product, catalog, needles, output):
    original = json.loads((product / "execution/graph.json").read_text())
    plant = json.loads((product / "plant-recipe.json").read_text())
    metadata = json.loads((catalog / "catalog.json").read_text())
    areas, hashes = cached_areas(needles)
    target = plant["authoring"]["target_area_m2"]
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    for name, recipe, settings in variants(original):
        summary = primary_summary(recipe)
        row = {"name": name, "settings": settings, "primary": summary}
        try:
            row |= measure(recipe, plant["physical_source"], metadata, areas)
            row["target_area_m2"] = target
            row["relative_area_error"] = abs(row["expected_area_m2"] - target) / target
            row["area_admitted"] = row["relative_area_error"] <= 0.05
        except ValueError as error:
            row["rejected"] = str(error)
        rows.append(row)
        (output / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")
        print(json.dumps(row), flush=True)
    (output / "preflight.json").write_text(json.dumps(rows, indent=2) + "\n")
    (output / "area-inputs.json").write_text(json.dumps(hashes, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("plant-product", "catalog", "needles", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    probe(args.plant_product, args.catalog, args.needles, args.output)
