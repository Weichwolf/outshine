"""Prepare an explicitly provisional parent-growth recipe and its physical ledger."""

import argparse
import hashlib
import json
import subprocess
from dataclasses import replace
from pathlib import Path

from mature_density import MAX_AXES, MAX_WINDOWS
from outshine_ref.vegetation import NeedledPlant
from parent_growth_area_probe import probe


def prepare(source, output, species, secondary, terminal, target_area):
    if not 0 < target_area < float("inf"):
        raise ValueError("finite positive physical area target required")
    plant = NeedledPlant.from_recipe(json.loads(source.read_text()))
    if len(plant.wood.levels) != 4:
        raise ValueError("four-order mature control input required")
    levels = list(plant.wood.levels)
    levels[2] = replace(levels[2], branches=secondary)
    levels[3] = replace(levels[3], branches=terminal)
    proposal = replace(plant, wood=replace(plant.wood, levels=tuple(levels)))
    output.mkdir(parents=True, exist_ok=True)
    path = output / "proposal-input.json"
    path.write_text(json.dumps(proposal.recipe(), indent=2) + "\n")
    report = probe(path, species)
    selected = report["alternatives"][-1]
    if selected["status"] != "measured":
        raise ValueError("proposed physical layout exceeds the axis budget")
    if sum(selected["axis_counts"]) > MAX_AXES or selected["windows"] > MAX_WINDOWS:
        raise ValueError("proposed physical layout exceeds the mature budgets")
    error = abs(selected["expected_area_m2"] - target_area) / target_area
    report.update(
        source_recipe=str(source),
        species=species,
        source_recipe_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        target_area_m2=target_area,
        relative_area_error=error,
        accepted_area_tolerance=0.05,
        area_target_met=error <= 0.05,
        selected_mode="relative-distance",
        proposal_implementation_sha256=hashlib.sha256(
            Path(__file__).read_bytes()
        ).hexdigest(),
        source_modified=report["source_modified"]
        or bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", __file__], text=True
            ).strip()
        ),
        scope="proposed physical parent-relative layout; stored v4 JSON is probe input only, not an executable parent-growth production recipe; geometry, botanical form and transitions unapproved",
    )
    (output / "proposal-area.json").write_text(json.dumps(report, indent=2) + "\n")
    return selected | {"relative_area_error": error, "area_target_met": error <= 0.05}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--species", choices=("picea-abies", "pinus-sylvestris"), required=True
    )
    parser.add_argument("--secondary", type=int, required=True)
    parser.add_argument("--terminal", type=int, required=True)
    parser.add_argument("--target-area", type=float, required=True)
    args = parser.parse_args()
    print(
        json.dumps(
            prepare(
                args.recipe,
                args.output,
                args.species,
                args.secondary,
                args.terminal,
                args.target_area,
            )
        )
    )
