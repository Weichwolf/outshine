"""Compare bounded, neighbor-stitched terrain patches on cached native pages."""

import argparse
import csv
import hashlib
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from current_visibility import box_bounds
from terrain_grid_lod import page_geometry, raster_view, stencil
from terrain_patch_lod import patch_bounds, projected_error


def balanced(levels):
    result = np.asarray(levels, dtype=np.int32).copy()
    if result.shape != (4, 4) or np.any(result < 0) or np.any(result > 3):
        raise ValueError("a page requires sixteen levels in [0, 3]")
    for _ in range(int(result.max())):
        before = result.copy()
        result[1:] = np.minimum(result[1:], before[:-1] + 1)
        result[:-1] = np.minimum(result[:-1], before[1:] + 1)
        result[:, 1:] = np.minimum(result[:, 1:], before[:, :-1] + 1)
        result[:, :-1] = np.minimum(result[:, :-1], before[:, 1:] + 1)
    return result


def boundaries(levels, row, column):
    neighbors = (
        (row - 1, column),
        (row, column + 1),
        (row + 1, column),
        (row, column - 1),
    )
    here = int(levels[row, column])
    return tuple(
        1 << min(here, int(levels[y, x]) if 0 <= y < 4 and 0 <= x < 4 else 0)
        for y, x in neighbors
    )


def stitched(
    fractions,
    local,
    nodes,
    distances,
    initial,
    focal,
    cosine,
    templates,
    balance_neighbors=True,
):
    levels = np.asarray(initial, dtype=np.int32).reshape(4, 4).copy()
    potential = int(levels.sum())
    evaluations = 0
    for iteration in range(potential + 1):
        if balance_neighbors:
            levels = balanced(levels)
        errors, counts = np.zeros(16), np.full(16, 128, dtype=np.int64)
        for patch, level in enumerate(levels.ravel()):
            if level == 0:
                continue
            row, column = divmod(patch, 4)
            fx, fy = (
                fractions[column * 8 : column * 8 + 9],
                fractions[row * 8 : row * 8 + 9],
            )
            fx, fy = (fx - fx[0]) / (fx[-1] - fx[0]), (fy - fy[0]) / (fy[-1] - fy[0])
            stride, edges = 1 << int(level), boundaries(levels, row, column)
            key = (fx.tobytes(), fy.tobytes(), stride, edges)
            if key not in templates:
                templates[key] = stencil(fx, stride, vertical=fy, boundary_steps=edges)
            indices, weights, counts[patch] = templates[key]
            delta = np.einsum("ijk,ij->ik", local[nodes[patch][indices]], weights)
            errors[patch] = np.linalg.norm(delta, axis=1).max() + 1e-9
            evaluations += 1
        projected = projected_error(
            np.column_stack((np.zeros(16), errors)), distances, focal, cosine
        )[:, 1]
        projected[levels.ravel() == 0] = 0
        unsafe = ~np.isfinite(projected) | (projected > 0.5)
        if not np.any(unsafe):
            return levels.ravel(), counts, projected, iteration, evaluations
        levels.ravel()[unsafe] -= 1
    raise AssertionError("each unsafe iteration must decrease a finite level potential")


def compare(pages, scenario, limit=0, balance_neighbors=True):
    started = time.perf_counter()
    with (pages / "pages.csv").open() as source:
        all_rows = list(csv.DictReader(source))
    views = [raster_view(scenario, all_rows, pages, 1280, 720, yaw) for yaw in (0, 180)]
    rows = (
        all_rows
        if not limit
        else [
            all_rows[i]
            for i in np.linspace(
                0, len(all_rows) - 1, min(limit, len(all_rows)), dtype=int
            )
        ]
    )
    focal = views[0][2]
    cosine = 1 / np.sqrt(1 + (1280**2 + 720**2) / (4 * focal**2))
    fingerprint = hashlib.sha256((pages / "pages.csv").read_bytes())
    results = [
        {
            "yaw": yaw,
            "baseline": 0,
            "candidate": 0,
            "selected": 0,
            "skirts": 0,
            "selected_pages": 0,
        }
        for yaw in (0, 180)
    ]
    templates, evaluations, iterations, processed = {}, 0, 0, 0
    for row in rows:
        fractions, local, _, world, stamp = page_geometry(row, pages)
        fingerprint.update(row["page"].encode() + stamp)
        page_visible = [
            bool(
                box_bounds((world.min(axis=0)[None], world.max(axis=0)[None]), v)[4][0]
            )
            for v in views
        ]
        if not any(page_visible):
            continue
        processed += 1
        errors, counts, nodes, strides = patch_bounds(fractions, local, 8, templates)
        low, high = world[nodes].min(axis=1), world[nodes].max(axis=1)
        distances = np.maximum(
            np.linalg.norm((low + high) / 2 - views[0][0], axis=1)
            - np.linalg.norm((high - low) / 2, axis=1),
            0,
        )
        projected = projected_error(errors, distances, focal, cosine)
        initial = np.max(np.where(projected <= 0.5, np.arange(len(strides)), 0), axis=1)
        before = counts[np.arange(16), initial]
        _, after, _, rounds, work = stitched(
            fractions,
            local,
            nodes,
            distances,
            initial,
            focal,
            cosine,
            templates,
            balance_neighbors,
        )
        evaluations += work
        iterations = max(iterations, rounds)
        for result, view, visible in zip(results, views, page_visible):
            mask = box_bounds((low, high), view)[4]
            baseline, candidate = int(mask @ before), int(mask @ after)
            result["baseline"] += baseline
            result["candidate"] += candidate
            result["selected"] += min(baseline, candidate)
            result["selected_pages"] += int(candidate < baseline)
            result["skirts"] += 256 * int(visible)
    for result in results:
        for key in ("baseline", "candidate", "selected"):
            result[key + "_with_skirts"] = result[key] + result["skirts"]
    patterns = {key[2:]: value[2] for key, value in templates.items() if len(key) == 4}
    return {
        "available_pages": len(all_rows),
        "sampled_pages": len(rows),
        "processed_pages": processed,
        "sample": "all" if not limit else "uniform page indices",
        "width": 1280,
        "height": 720,
        "pixel_allowance": 0.5,
        "balanced_neighbors": balance_neighbors,
        "cases": results,
        "distinct_stitched_patterns": len(patterns),
        "stitched_index_bytes": sum(patterns.values()) * 12,
        "exact_variant_evaluations": evaluations,
        "maximum_refinement_rounds": iterations,
        "template_count": len(templates),
        "seconds": time.perf_counter() - started,
        "input_sha256": fingerprint.hexdigest(),
        "scenario_sha256": hashlib.sha256(scenario.read_bytes()).hexdigest(),
        "source_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "terrain_patch_stitching.py",
                "terrain_grid_lod.py",
                "terrain_patch_lod.py",
                "current_visibility.py",
                "terrain_visibility.py",
            )
        },
        "scope": "Exact common-refinement vector bounds; common neighbor edges, original page edges/skirts. Whole-page fallback cannot increase submitted references in this frustum-only model. Selection work, native GPU execution, normals and contact/temporal pixels remain unproved.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pages", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--unbalanced", action="store_true")
    args = parser.parse_args()
    if args.limit < 0 or not args.output.resolve().is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("limit must be nonnegative; output belongs in system temp")
    report = compare(args.pages, args.scenario, args.limit, not args.unbalanced)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
