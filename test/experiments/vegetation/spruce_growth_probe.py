"""Count bounded existing parent profiles before executing a new spruce crown."""

import argparse
import copy
import hashlib
import json
import time
from pathlib import Path

import numpy as np
from conifer_windows import pattern_key
from outshine_ref.vegetation import NeedlePattern
from parent_profile_graph import ParentProfileGraph

MAX_AXES = 60000
MAX_WINDOWS = 80000


def variant(original, width, whorls, variance=0.12, spread=0.08, seed=None):
    if (
        not np.isfinite(width)
        or not 1 <= width <= 2
        or type(whorls) is not int
        or not 2 <= whorls <= 48
    ):
        raise ValueError(
            "bounded declared crown width and complete primary whorls required"
        )
    recipe = copy.deepcopy(original)
    if seed is not None:
        recipe["seed"] = seed
    node = recipe["nodes"][2]
    node["shape"]["length"] *= width
    node["shape"]["radius"]["table"] = [
        r * width for r in node["shape"]["radius"]["table"]
    ]
    placement = node["placement"]
    placement["scale_variance"] = variance
    pattern = placement["pattern"]
    pattern["limit_groups"] = whorls
    pattern["spacing"]["table"] = [
        (placement["last"] - placement["first"])
        * recipe["nodes"][1]["shape"]["length"]
        / (whorls - 1)
    ] * 4
    pattern["spread_m"] = spread
    return recipe


def cached_areas(directory):
    scene = json.loads((directory / "shoot.json").read_text())
    result, hashes = [], {}
    for item in scene["products"]:
        if item["surface"] != "leaf":
            continue
        path = directory / item["mesh"]
        with np.load(path, allow_pickle=False) as data:
            points = data["vertices"][data["faces"]]
        result.append(
            float(
                np.linalg.norm(
                    np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0]),
                    axis=1,
                ).sum()
                / 4
            )
        )
        hashes[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    return np.array(result), hashes


def expected_area(counts, patterns, seed, areas):
    total, first = 0.0, 0
    for count, pattern in zip(counts, patterns, strict=True):
        cycles, remainder = divmod(count, len(areas))
        variants = np.full(len(areas), cycles, dtype=np.int64)
        for index in range(remainder):
            variants[(first + seed + index) % len(areas)] += 1
        total += float(variants @ areas) * (
            1 + pattern.scale_variation**2 / 3 if pattern else 1
        )
        first += count
    return total


def measure(recipe, physical_source, catalog, areas, *, include_lengths=False):
    start = time.perf_counter()
    graph = ParentProfileGraph.from_recipe(recipe)
    if set(graph.by_id) != set(range(5)) or any(
        graph.by_id[i].kind != "axis" for i in range(1, 5)
    ):
        raise ValueError("one explicit four-order spruce graph required")
    patterns = [
        NeedlePattern.from_recipe(p) if p else None for p in physical_source["patterns"]
    ]
    if (
        len(patterns) != 4
        or graph.by_id[3].placement.scale_variance
        or graph.by_id[4].placement.scale_variance
    ):
        raise ValueError(
            "four orders and deterministic secondary/terminal length laws required"
        )
    if (
        graph.by_id[4].placement.pattern is not None
        or graph.by_id[4].placement.count != 1
    ):
        raise ValueError("one complete terminal per secondary parent required")
    primary = graph.instances(2, MAX_AXES)
    if primary is None:
        raise ValueError("primary graph exceeds the pose budget")
    secondary = []
    for parent in primary:
        sites = graph.site_table(3, parent)
        secondary.append(
            parent.length
            * graph.ratios[3]
            * graph.by_id[3].placement.scale.evaluate(sites[:, 0])
        )
    lengths = [
        np.array([graph.instance(((0, 0), (1, 0))).length]),
        np.array([p.length for p in primary]),
        np.concatenate(secondary),
    ]
    terminal = graph.by_id[4].placement
    lengths.append(
        lengths[2]
        * graph.ratios[4]
        * float(terminal.scale.evaluate(terminal.parameter(0)))
    )
    axes = sum(map(len, lengths))
    if axes > MAX_AXES:
        raise ValueError("full count-only crown exceeds the unchanged axis budget")
    counts, windows = [], 0
    for pattern, values in zip(patterns, lengths, strict=True):
        if pattern is None:
            counts.append(0)
            continue
        if np.any((values < 1e-6) | (values > 1000)):
            raise ValueError("physical twig lengths must remain in the original range")
        groups = (
            np.floor((pattern.last - pattern.first) * values / pattern.spacing).astype(
                np.uint64
            )
            + 1
        )
        if np.any(groups > 4096):
            raise ValueError("physical needle capacity must remain bounded")
        local = groups * pattern.group_size
        capacity = catalog[pattern_key(pattern)]["needles"]
        windows += int(((local + capacity - 1) // capacity).sum())
        counts.append(int(local.sum()))
    if windows > MAX_WINDOWS:
        raise ValueError("full count-only crown exceeds the unchanged window budget")
    result = {
        "graph_identity": graph.identity,
        "axes": axes,
        "orders": [len(x) for x in lengths],
        "windows": windows,
        "needle_counts": counts,
        "expected_area_m2": expected_area(counts, patterns, graph.seed, areas),
        "count_only_s": time.perf_counter() - start,
        "prepared_poses": len(graph._poses),
    }
    return (result, lengths) if include_lengths else result


def probe(original, physical_source, catalog, areas, output):
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    cases = [(1.0, 48, 0.0, 0.0, 8.0)] + [
        (width, groups, 0.12, 0.08, 5.0)
        for width in (1.3, 1.45, 1.6)
        for groups in (28, 32, 36, 40)
    ]
    cases += [(width, 34, 0.12, 0.08, 5.0) for width in (1.35, 1.4, 1.45)]
    for index, (width, whorls, variance, spread, lai) in enumerate(cases):
        recipe = (
            copy.deepcopy(original)
            if index == 0
            else variant(original, width, whorls, variance, spread)
        )
        row = {
            "index": index,
            "width_factor": width,
            "primary_whorls": whorls,
            "length_variance": variance,
            "attachment_spread_m": spread,
            "nominal_lai": lai,
            "nominal_crown_width_m": 5.7 * width,
        }
        row["target_area_m2"] = lai * np.pi * (row["nominal_crown_width_m"] / 2) ** 2
        try:
            row |= measure(recipe, physical_source, catalog, areas)
            row["relative_area_error"] = (
                abs(row["expected_area_m2"] - row["target_area_m2"])
                / row["target_area_m2"]
            )
            (output / f"graph-{index}.json").write_text(
                json.dumps(recipe, indent=2) + "\n"
            )
        except ValueError as error:
            row["rejected"] = str(error)
        rows.append(row)
        print(json.dumps(row), flush=True)
    (output / "counts.json").write_text(json.dumps(rows, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("graph", "plant", "catalog", "needles", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    areas, hashes = cached_areas(args.needles)
    source = json.loads((args.plant / "plant-recipe.json").read_text())[
        "physical_source"
    ]
    probe(
        json.loads(args.graph.read_text()),
        source,
        json.loads((args.catalog / "catalog.json").read_text()),
        areas,
        args.output,
    )
    (args.output / "area-inputs.json").write_text(json.dumps(hashes, indent=2) + "\n")
