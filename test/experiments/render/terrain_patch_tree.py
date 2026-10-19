"""Choose bounded shared terrain patterns using a bottom-up spatial cost model."""

import argparse
import csv
import hashlib
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from current_visibility import box_bounds
from terrain_grid_lod import page_geometry, raster_view
from terrain_patch_lod import patch_bounds, projected_error


def solve(costs):
    if len(costs) != 3 or any(np.shape(c) != (n,) for c, n in zip(costs, (16, 4, 1))):
        raise ValueError("costs must describe 8/16/32-cell quadtree levels")
    if any(np.any(c < 0) for c in costs):
        raise ValueError("costs must be nonnegative")
    totals = [np.asarray(costs[0])]
    merged = [np.ones(16, dtype=bool)]
    for level in (1, 2):
        side = 4 >> (level - 1)
        children = totals[-1].reshape(side // 2, 2, side // 2, 2).sum(axis=(1, 3)).ravel()
        merged.append(costs[level] < children)
        totals.append(np.minimum(costs[level], children))
    selected = []

    def descend(level, row, col):
        index = row * (4 >> level) + col
        if merged[level][index]:
            if costs[level][index] > 0:
                selected.append((level, index))
        else:
            for y in (0, 1):
                for x in (0, 1):
                    descend(level - 1, 2 * row + y, 2 * col + x)

    descend(2, 0, 0)
    return int(totals[-1][0]), selected


def compare(pages, scenario, width, height):
    started = time.perf_counter()
    with (pages / "pages.csv").open() as source:
        rows = list(csv.DictReader(source))
    views = [raster_view(scenario, rows, pages, width, height, yaw) for yaw in (0, 180)]
    focal = views[0][2]
    cosine = 1 / np.sqrt(1 + (width * width + height * height) / (4 * focal * focal))
    fingerprint = hashlib.sha256((pages / "pages.csv").read_bytes())
    templates = {}
    results = [dict(yaw=yaw, baseline_interiors=0, selected_interiors=0, skirts=0, nodes_by_cells={8: 0, 16: 0, 32: 0}) for yaw in (0, 180)]
    processed = 0
    for row in rows:
        fractions, local, _, world, stamp = page_geometry(row, pages)
        fingerprint.update(stamp)
        whole = world.min(axis=0)[None], world.max(axis=0)[None]
        page_visible = [box_bounds(whole, view)[4][0] for view in views]
        if not any(page_visible):
            continue
        processed += 1
        choices = [[], []]
        for cells in (8, 16, 32):
            errors, counts, nodes, strides = patch_bounds(fractions, local, cells, templates)
            low, high = world[nodes].min(axis=1), world[nodes].max(axis=1)
            distance = np.maximum(
                np.linalg.norm((low + high) / 2 - views[0][0], axis=1)
                - np.linalg.norm((high - low) / 2, axis=1),
                0,
            )
            projected = projected_error(errors, distance, focal, cosine)
            chosen = np.max(np.where(projected <= 0.5, np.arange(len(strides)), 0), axis=1)
            costs = counts[np.arange(len(nodes)), chosen]
            for view, options in zip(views, choices):
                visible = box_bounds((low, high), view)[4]
                options.append(np.where(visible, costs, 0))
        for result, options, visible in zip(results, choices, page_visible):
            cost, nodes = solve(options)
            result["baseline_interiors"] += int(options[0].sum())
            result["selected_interiors"] += cost
            result["skirts"] += 256 * int(visible)
            for level, _ in nodes:
                result["nodes_by_cells"][8 << level] += 1
    for result in results:
        result["baseline_with_skirts"] = result["baseline_interiors"] + result["skirts"]
        result["selected_with_skirts"] = result["selected_interiors"] + result["skirts"]
    return dict(
        pages=len(rows), processed_pages=processed, width=width, height=height,
        pixel_allowance=0.5, cases=results, seconds=time.perf_counter() - started,
        input_sha256=fingerprint.hexdigest(),
        scenario_sha256=hashlib.sha256(scenario.read_bytes()).hexdigest(),
        source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        scope="Minimum triangle count within the tested 8/16/32-cell patterns and fixed per-node bounds. All selected boundaries and outer skirts retained; no depth rejection. Not a global optimum or native performance/normal/pixel proof.",
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pages", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("output must be in system temp")
    report = compare(args.pages, args.scenario, 1280, 720)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
