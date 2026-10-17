"""Count a branching twig hierarchy before any complete fine pose or needle work."""

import argparse
import copy
import json
from dataclasses import replace
from pathlib import Path

import numpy as np
from conifer_windows import pattern_key
from outshine_ref.vegetation import NeedlePattern
from outshine_ref.vegetation.placement_patterns import MAX_SITES
from parent_profile_graph import ParentProfileGraph
from spruce_branch_planes import branch_form
from spruce_growth_probe import MAX_AXES, MAX_WINDOWS, cached_areas, expected_area
from spruce_study_source import INPUT, read_input
from spruce_whorl_balance import balanced


def branching(original, spacing=0.04, *, posture="hanging"):
    if (
        not np.isfinite(spacing)
        or not 0.025 <= spacing <= 0.08
        or type(posture) is not str
        or posture not in ("hanging", "radial")
    ):
        raise ValueError(
            "bounded physical end-branch spacing and explicit posture required"
        )
    recipe = branch_form(
        balanced(original, 48, 1.6), planar=True, fixed=True, hanging=True
    )
    support, tip = recipe["nodes"][3:5]
    support["placement"]["pattern"]["spacing"]["table"] = [0.15] * 4
    support["placement"]["pattern"]["limit_groups"] = 64
    # Preserve each previous physical attachment radius at equal parent length
    # and local scale when changing the length ratios.
    old_ratios = dict(recipe["parent_length_ratios"])
    for node, ratio in ((support, 0.5), (tip, 0.7)):
        node["shape"]["radius"]["table"] = (
            np.array(node["shape"]["radius"]["table"]) * old_ratios[node["id"]] / ratio
        ).tolist()
    recipe["parent_length_ratios"] = [[3, 0.5], [4, 0.7]]
    tip["placement"]["pattern"] = copy.deepcopy(support["placement"]["pattern"])
    pattern = tip["placement"]["pattern"]
    pattern["spacing"]["table"] = [spacing] * 4
    pattern["divergence"] = (
        0.0 if posture == "hanging" else float(np.pi * (3 - np.sqrt(5)))
    )
    if posture == "hanging":
        tip["placement"]["phase"] = {
            "version": "fixed-parent-azimuth-1",
            "radians": -float(np.pi) / 2,
        }
    tip["placement"]["normal_reference"] = {"version": "world-up-axis-normal-1"}
    tip["placement"]["azimuth_variance"] = 0.25
    tip["placement"]["scale"]["table"] = np.linspace(1.05, 0.25, 4).tolist()
    tip["shape"]["bend"] = -0.35
    return recipe


def child_lengths(graph, node_id, parents, budget):
    node = graph.by_id[node_id]
    placement = node.placement
    if placement.scale_variance or budget < 0:
        raise ValueError(
            "deterministic inherited lengths and positive work budget required"
        )
    if placement.pattern is None:
        if placement.count != 1:
            raise ValueError(
                "only the existing single continuation needs no distance pattern"
            )
        if len(parents) > budget:
            raise ValueError("complete continuation exceeds the unchanged axis budget")
        values = (
            parents
            * graph.ratios[node_id]
            * float(placement.scale.evaluate(placement.parameter(0)))
        )
        return node.shape.length * (values / node.shape.length)
    pattern = placement.pattern
    if pattern.spread_m or not np.all(
        pattern.spacing.table == pattern.spacing.table[0]
    ):
        raise ValueError(
            "explicit constant physical spacing without longitudinal jitter required"
        )
    uncapped = (
        pattern
        if pattern.mode == "uniform"
        else replace(pattern, limit_groups=MAX_SITES // pattern.whorl_size)
    )
    desired = np.array(
        [
            uncapped.capacity(float(length), placement.first, placement.last)
            for length in parents
        ]
    )
    if np.any(desired > pattern.limit_groups * pattern.whorl_size):
        raise ValueError("no silently capped supporting or end branch admitted")
    tables = pattern.tables(parents, placement.first, placement.last, max_sites=budget)
    if tables is None:
        raise ValueError("complete child population exceeds unchanged axis budget")
    counts = np.array([len(t) for t in tables])
    np.testing.assert_array_equal(counts, desired)
    parameter = np.concatenate([t[:, 0] for t in tables])
    values = (
        np.repeat(parents, counts)
        * graph.ratios[node_id]
        * placement.scale.evaluate(parameter)
    )
    return node.shape.length * (values / node.shape.length)


def preflight(recipe, physical, metadata, areas):
    graph = ParentProfileGraph.from_recipe(recipe)
    if set(graph.by_id) != set(range(5)):
        raise ValueError("same complete four-order physical graph required")
    primary = graph.instances(2, 1000)
    if primary is None or len(primary) != 240:
        raise ValueError("same complete240main branches required")
    values = [
        np.array([graph.instance(((0, 0), (1, 0))).length]),
        np.array([p.length for p in primary]),
    ]
    for key in (3, 4):
        values.append(
            child_lengths(graph, key, values[-1], MAX_AXES - sum(map(len, values)))
        )
    patterns = [
        NeedlePattern.from_recipe(p) if p else None for p in physical["patterns"]
    ]
    counts, windows = [], 0
    for pattern, local in zip(patterns, values, strict=True):
        if pattern is None:
            counts.append(0)
            continue
        if np.any(local < 1e-6) or np.any(local > 1000):
            raise ValueError("bounded positive physical lengths required")
        groups = (
            np.floor((pattern.last - pattern.first) * local / pattern.spacing).astype(
                np.uint64
            )
            + 1
        )
        if np.any(groups > 4096):
            raise ValueError("unchanged per-axis needle limit required")
        local_counts = groups * pattern.group_size
        capacity = metadata[pattern_key(pattern)]["needles"]
        windows += int(((local_counts + capacity - 1) // capacity).sum())
        counts.append(int(local_counts.sum()))
    if windows > MAX_WINDOWS:
        raise ValueError("complete physical windows exceed unchanged work budget")
    return {
        "graph_identity": graph.identity,
        "axes": sum(map(len, values)),
        "orders": [len(v) for v in values],
        "windows": windows,
        "needle_counts": counts,
        "expected_area_m2": expected_area(counts, patterns, graph.seed, areas),
        "prepared_poses": len(graph._poses),
        "maximum_length_m_by_order": [float(v.max()) for v in values],
        "capacity_counts": dict(graph.capacities),
    }


def probe(source, output):
    data, original = read_input(INPUT)
    metadata = json.loads((source / "catalog/catalog.json").read_text())
    areas, hashes = cached_areas(source / "models")
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    cases = (
        [("control", balanced(original, 48, 1.6))]
        + [(f"hanging-{s}", branching(original, s)) for s in (0.035, 0.04, 0.045)]
        + [("radial-0.045", branching(original, 0.045, posture="radial"))]
    )
    for name, recipe in cases:
        row = {"name": name}
        try:
            row |= preflight(recipe, data["physical_source"], metadata, areas)
            error = abs(
                row["expected_area_m2"] / data["authoring"]["target_area_m2"] - 1
            )
            row |= {"relative_area_error": error, "area_admitted": error <= 0.05}
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
