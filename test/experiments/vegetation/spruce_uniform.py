"""Pre-count bounded complete-interval branchlets and their primary-owned area."""

import argparse
import json
from pathlib import Path

import numpy as np
from outshine_ref.vegetation import NeedlePattern
from parent_profile_graph import ParentProfileGraph
from spruce_fine_hierarchy import branching, child_lengths, preflight
from spruce_growth_probe import MAX_AXES, cached_areas
from spruce_study_source import INPUT, read_input
from spruce_whorl_balance import balanced


def uniform(original, groups=19, spacing=0.015):
    if type(groups) is not int or not 1 <= groups <= 64:
        raise ValueError("one to64 complete terminal groups required")
    if not np.isfinite(spacing) or not 0.001 <= spacing <= 0.1:
        raise ValueError("bounded positive physical minimum spacing required")
    recipe = branching(original, 0.045, posture="radial")
    pattern = recipe["nodes"][4]["placement"]["pattern"]
    pattern.update(mode="uniform", limit_groups=groups)
    pattern["spacing"]["table"] = [spacing] * 4
    return recipe


def span_areas(counts, first_ids, areas, seed, variance):
    """Expected actual cyclic organ-family area without enumerating needles."""
    size = len(areas)
    cycles, tails = np.divmod(counts, size)
    starts = (first_ids + seed) % size
    prefix = np.r_[0.0, np.cumsum(np.tile(areas, 2))]
    return (cycles * float(np.sum(areas)) + prefix[starts + tails] - prefix[starts]) * (
        1 + variance**2 / 3
    )


def inventory(recipe, physical, areas):
    graph = ParentProfileGraph.from_recipe(recipe)
    primary = graph.instances(2, 1000)
    lengths = np.array([p.length for p in primary])
    heights = np.array([p.origin[2] for p in primary])
    owners = np.arange(len(primary))
    mass = np.zeros(len(primary))
    total, first = len(primary) + 1, 0
    for order in (1, 2, 3):
        pattern = NeedlePattern.from_recipe(physical["patterns"][order])
        counts = (
            np.floor((pattern.last - pattern.first) * lengths / pattern.spacing).astype(
                np.int64
            )
            + 1
        )
        counts *= pattern.group_size
        starts = first + np.r_[0, np.cumsum(counts[:-1])]
        values = span_areas(counts, starts, areas, graph.seed, pattern.scale_variation)
        np.add.at(mass, owners, values)
        first += int(counts.sum())
        if order < 3:
            placement = graph.by_id[order + 2].placement
            if placement.pattern is not None:
                tables = placement.pattern.tables(
                    lengths, placement.first, placement.last, max_sites=MAX_AXES - total
                )
                if tables is None:
                    raise ValueError(
                        "complete spatial inventory exceeds unchanged budget"
                    )
                owners = np.repeat(owners, [len(t) for t in tables])
            lengths = child_lengths(graph, order + 2, lengths, MAX_AXES - total)
            total += len(lengths)
    return {
        "primary_attachment_heights_m": heights.tolist(),
        "primary_owned_expected_area_m2": mass.tolist(),
        "owned_area_above20m_m2": float(mass[heights > 20].sum()),
        "owned_area_above25m_m2": float(mass[heights > 25].sum()),
        "owned_total_area_m2": float(mass.sum()),
    }


def probe(source, output):
    data, original = read_input(INPUT)
    metadata = json.loads((source / "catalog/catalog.json").read_text())
    areas, hashes = cached_areas(source / "models")
    output.mkdir(parents=True, exist_ok=False)
    cases = [
        ("control", balanced(original, 48, 1.6)),
        ("radial", branching(original, 0.045, posture="radial")),
    ] + [(f"uniform-{n}", uniform(original, n)) for n in (16, 17, 18, 19, 20, 24)]
    rows = []
    for name, recipe in cases:
        row = {"name": name}
        try:
            row |= preflight(recipe, data["physical_source"], metadata, areas)
            row |= inventory(recipe, data["physical_source"], areas)
            error = abs(
                row["expected_area_m2"] / data["authoring"]["target_area_m2"] - 1
            )
            row |= {"area_admitted": error <= 0.05, "relative_area_error": error}
        except ValueError as error:
            row["rejected"] = str(error)
        (output / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")
        rows.append(row)
        print(
            json.dumps({k: v for k, v in row.items() if not k.startswith("primary_")}),
            flush=True,
        )
    (output / "preflight.json").write_text(json.dumps(rows, indent=2) + "\n")
    (output / "area-inputs.json").write_text(json.dumps(hashes, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    probe(args.source, args.output)
