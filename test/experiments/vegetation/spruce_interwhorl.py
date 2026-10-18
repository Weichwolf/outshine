"""Two existing primary populations feeding one parent-dependent fine hierarchy."""

import argparse
import copy
import json
from pathlib import Path

import numpy as np
from conifer_windows import pattern_key
from outshine_ref.vegetation import NeedlePattern
from parent_profile_graph import ParentProfileGraph
from spruce_growth_probe import MAX_AXES, MAX_WINDOWS, cached_areas, expected_area
from spruce_study_source import INPUT, read_input
from spruce_whorl_balance import balanced


def intercalated(original, spacing_factor, *, relative_length=0.7):
    if (
        not np.isfinite([spacing_factor, relative_length]).all()
        or not 1.2 <= spacing_factor <= 2
        or not 0.4 <= relative_length <= 0.8
    ):
        raise ValueError("bounded physical spacing and smaller primary length required")
    recipe = balanced(original, 48, spacing_factor)
    main = recipe["nodes"][2]
    main["placement"]["pattern"]["whorl_size"] = 4
    small = copy.deepcopy(main)
    small["id"], small["label"] = 5, "smaller intercalated primaries"
    placement = small["placement"]
    # This population is distributed independently, not claimed to coincide
    # with the exact midpoint of every irregular annual internode.
    interval = (placement["last"] - placement["first"]) / 47
    placement["first"] += interval / 2
    placement["last"] -= interval / 2
    placement["count"] = 47
    placement.pop("pattern")
    placement["scale"]["table"] = (
        np.array(placement["scale"]["table"]) * relative_length
    ).tolist()
    recipe["nodes"].append(small)
    recipe["nodes"][3]["parents"] = [2, 5]
    recipe["nodes"][3]["placement"]["pattern"]["limit_groups"] = 208
    recipe["inclinations"].append(
        [
            5,
            copy.deepcopy(
                next(value for key, value in recipe["inclinations"] if key == 2)
            ),
        ]
    )
    return recipe


def lengths(recipe):
    graph = ParentProfileGraph.from_recipe(recipe)
    primaries = []
    for key in (2, 5):
        if key in graph.by_id:
            poses = graph.instances(key, 1000)
            if poses is None:
                raise ValueError(
                    "bounded primary populations required before fine work"
                )
            primaries.extend(poses)
    fine = graph.by_id[3].placement
    rows, capped = [], 0
    for parent in primaries:
        sites = graph.site_table(3, parent)
        desired = (
            int(
                np.floor(
                    (fine.last - fine.first)
                    * parent.length
                    / fine.pattern.spacing.table[0]
                )
            )
            + 1
        )
        capped += desired > fine.pattern.limit_groups
        rows.append(parent.length * graph.ratios[3] * fine.scale.evaluate(sites[:, 0]))
    values = [
        np.array([graph.instance(((0, 0), (1, 0))).length]),
        np.array([p.length for p in primaries]),
        np.concatenate(rows),
    ]
    terminal = graph.by_id[4].placement
    values.append(
        values[2]
        * graph.ratios[4]
        * float(terminal.scale.evaluate(terminal.parameter(0)))
    )
    if capped:
        raise ValueError("complete fine support required; secondary cap was reached")
    return graph, primaries, values


def preflight(recipe, physical, metadata, areas):
    graph, primary, values = lengths(recipe)
    fine, terminal = graph.by_id[3].placement, graph.by_id[4].placement
    if (
        set(graph.by_id) not in (set(range(5)), set(range(6)))
        or fine.scale_variance
        or terminal.scale_variance
        or terminal.pattern is not None
        or terminal.count != 1
        or not np.all(fine.pattern.spacing.table == fine.pattern.spacing.table[0])
    ):
        raise ValueError("explicit deterministic four-order physical study required")
    if sum(map(len, values)) > MAX_AXES:
        raise ValueError("unchanged complete axis budget exceeded")
    patterns = [
        NeedlePattern.from_recipe(p) if p else None for p in physical["patterns"]
    ]
    counts, windows = [], 0
    for pattern, local in zip(patterns, values, strict=True):
        if pattern is None:
            counts.append(0)
            continue
        groups = (
            np.floor((pattern.last - pattern.first) * local / pattern.spacing).astype(
                np.uint64
            )
            + 1
        )
        if np.any(groups > 4096) or np.any(local < 1e-6):
            raise ValueError("unchanged bounded physical needle support required")
        count = groups * pattern.group_size
        capacity = metadata[pattern_key(pattern)]["needles"]
        windows += int(((count + capacity - 1) // capacity).sum())
        counts.append(int(count.sum()))
    if windows > MAX_WINDOWS:
        raise ValueError("unchanged complete window budget exceeded")
    return {
        "graph_identity": graph.identity,
        "axes": sum(map(len, values)),
        "orders": [len(v) for v in values],
        "windows": windows,
        "needle_counts": counts,
        "expected_area_m2": expected_area(counts, patterns, graph.seed, areas),
        "prepared_poses": len(graph._poses),
        "primary_by_generator": {
            str(key): sum(p.path[-1][0] == key for p in primary) for key in (2, 5)
        },
        "primary_max_length_m": max(p.length for p in primary),
        "capped_primary_axes": 0,
    }


def probe(source, output):
    data, original = read_input(INPUT)
    metadata = json.loads((source / "catalog/catalog.json").read_text())
    areas, hashes = cached_areas(source / "models")
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    # A small area admission bracket, not an appearance/profile sweep.
    cases = [("control", balanced(original, 48, 1.6))] + [
        (f"intercalated-{factor}", intercalated(original, factor))
        for factor in (1.4, 1.45, 1.5)
    ]
    for name, recipe in cases:
        row = {"name": name}
        try:
            row |= preflight(recipe, data["physical_source"], metadata, areas)
            error = abs(
                row["expected_area_m2"] / data["authoring"]["target_area_m2"] - 1
            )
            row |= {"relative_area_error": error, "admitted": error <= 0.05}
        except ValueError as error:
            row["rejected"] = str(error)
        (output / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")
        rows.append(row)
        print(json.dumps(row), flush=True)
    (output / "preflight.json").write_text(json.dumps(rows, indent=2) + "\n")
    (output / "area-inputs.json").write_text(json.dumps(hashes, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    probe(args.source, args.output)
