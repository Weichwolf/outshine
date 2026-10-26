"""Measure exact early patches plus unchanged bounded terrain LOD and depth rejection."""

import argparse
import csv
import hashlib
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from current_visibility import box_bounds, occluded, project, raster
from terrain_grid_lod import page_geometry, raster_view, triangles
from terrain_patch_lod import patch_bounds, projected_error
from terrain_visibility import partition


def prepare(pages, scenario, width, height, yaw):
    with (pages / "pages.csv").open() as source:
        rows = list(csv.DictReader(source))
    view = raster_view(scenario, rows, pages, width, height, yaw)
    cosine = 1 / np.sqrt(1 + (width * width + height * height) / (4 * view[2] ** 2))
    _, nodes = partition(8)
    fingerprint = hashlib.sha256((pages / "pages.csv").read_bytes())
    records, boxes, levels, counts, templates = [], [], [], [], {}
    for row in rows:
        fractions, local, _, world, stamp = page_geometry(row, pages)
        fingerprint.update(stamp)
        low, high = world[nodes].min(axis=1), world[nodes].max(axis=1)
        if not box_bounds((low, high), view)[4].any():
            continue
        errors, sizes, _, strides = patch_bounds(fractions, local, 8, templates)
        distance = np.maximum(
            np.linalg.norm((low + high) / 2 - view[0], axis=1)
            - np.linalg.norm((high - low) / 2, axis=1),
            0,
        )
        error = projected_error(errors, distance, view[2] * 720 / height, cosine)
        selected = np.max(np.where(error <= 0.5, np.arange(len(strides)), 0), axis=1)
        records.append(project(world, view))
        boxes.append((low, high))
        levels.append(selected)
        counts.append(sizes[np.arange(len(nodes)), selected])
    bounds = box_bounds(tuple(np.concatenate([b[k] for b in boxes]) for k in (0, 1)), view)
    return records, bounds, np.concatenate(levels), np.concatenate(counts), {
        "pages": len(rows),
        "frustum_pages": len(records),
        "input_sha256": fingerprint.hexdigest(),
        "scenario_sha256": hashlib.sha256(scenario.read_bytes()).hexdigest(),
    }


def surface_coverage(records, width, height):
    faces, _ = partition(8)
    result = []
    for projected, z in records:
        p = projected[faces]
        a, b, c = p[:, :, 0, :2], p[:, :, 1, :2], p[:, :, 2, :2]
        determinant = (b[..., 0] - a[..., 0]) * (c[..., 1] - a[..., 1]) - (b[..., 1] - a[..., 1]) * (c[..., 0] - a[..., 0])
        low, high = p[..., :2].min(axis=2), p[..., :2].max(axis=2)
        clipped_area = np.prod(np.maximum(np.minimum(high, (width, height)) - np.maximum(low, 0), 0), axis=2)
        active = (determinant < -1e-12) & (z[faces] > 0.1).all(axis=2)
        result.append(np.where(active, np.minimum(-determinant / 2, clipped_area), 0).sum(axis=1))
    return np.concatenate(result)


def rank(bounds, width, height, priority, surface=None):
    low, high, _, safe, visible, near = bounds
    area = np.prod(np.maximum(np.minimum(high, (width, height)) - np.maximum(low, 0), 0), axis=1)
    area = np.where(safe, area, width * height)
    ids = np.flatnonzero(visible)
    if priority == "distance":
        return ids[np.lexsort((-area[ids], near[ids]))]
    if priority == "surface":
        ids = ids[surface[ids] > 0]
        area = surface
    return ids[np.lexsort((near[ids], -area[ids]))]


def draw(records, mask, levels, width, height):
    _, nodes = partition(8)
    faces = [nodes[:, triangles(9, stride)] for stride in (1, 2, 4, 8)]
    depth = np.zeros((height, width))
    for page, (projected, z) in enumerate(records):
        active = mask[page * 16 : (page + 1) * 16]
        if not active.any():
            continue
        chosen = levels[page * 16 : (page + 1) * 16]
        selected = [f[active & (chosen == level)].reshape(-1, 3) for level, f in enumerate(faces)]
        depth = np.maximum(depth, raster(projected, z, np.concatenate(selected), width, height))
    return depth


def evaluate(records, bounds, levels, counts, width, height, budgets, priority):
    if not records or not budgets or min(width, height, *budgets) <= 0:
        raise ValueError("nonempty geometry and positive dimensions/budgets required")
    surface = surface_coverage(records, width, height) if priority == "surface" else None
    order = rank(bounds, width, height, priority, surface)
    low, high, nearest, safe, visible, _ = bounds
    results, images = [], {}
    early = np.zeros_like(visible)
    depth = np.zeros((height, width))
    fine = np.zeros_like(levels)
    for budget in sorted(set(budgets)):
        added = np.zeros_like(visible)
        added[order[:budget]] = True
        added &= ~early
        early |= added
        depth = np.maximum(depth, draw(records, added, fine, width, height))
        hidden = occluded(low - 1, high + 1, nearest, safe, depth, 10) & visible & ~early
        kept = visible & ~hidden & ~early
        actual = np.where(early, 0, levels)
        reference = draw(records, visible, actual, width, height)
        retained = np.maximum(depth, draw(records, kept, levels, width, height))
        difference = np.abs(reference - retained)
        early_triangles = int(early.sum()) * 128
        late_triangles = int(counts[kept].sum())
        total = early_triangles + late_triangles
        baseline = early_triangles + int(counts[visible & ~early].sum())
        results.append({
            "budget": budget,
            "early_patches": int(early.sum()),
            "hidden_patches": int(hidden.sum()),
            "early_triangles": early_triangles,
            "late_triangles": late_triangles,
            "total_interior_triangles": total,
            "same_geometry_unculled_triangles": baseline,
            "saved_interior_triangles": baseline - total,
            "changed_depth_pixels": int(np.count_nonzero(difference > 1e-12)),
            "max_depth_difference": float(difference.max()),
            "early_covered_pixels": int(np.count_nonzero(depth)),
        })
        images[budget] = (reference, retained, depth.copy())
    return results, images


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pages", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=180)
    parser.add_argument("--yaw", type=float, default=0)
    parser.add_argument("--budgets", type=int, nargs="+", default=[32, 128, 512])
    parser.add_argument("--priority", choices=("distance", "coverage", "surface"), default="coverage")
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("output must be in system temp")
    started = time.perf_counter()
    args.output.mkdir(parents=True, exist_ok=True)
    records, bounds, levels, counts, report = prepare(
        args.pages, args.scenario, args.width, args.height, args.yaw
    )
    results, images = evaluate(records, bounds, levels, counts, args.width, args.height, args.budgets, args.priority)
    skirts = len(records) * 256
    for result in results:
        result["unchanged_outer_skirt_triangles"] = skirts
        result["total_with_skirts"] = result["total_interior_triangles"] + skirts
    report.update(
        width=args.width, height=args.height, yaw=args.yaw, priority=args.priority,
        pixel_allowance_at_720p=0.5,
        adaptive_only_interior_triangles=int(counts[bounds[4]].sum()),
        cases=results, seconds=time.perf_counter() - started,
        source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        scope="Pixel-centre CPU model, exact fine early patches drawn once; late LOD bound fixed at 0.5 px at 720p. Each reference uses identical geometry to its culled case. Skirts counted unchanged but not rasterized; normal shading, near clipping, MSAA, native scheduling and net GPU cost unproved.",
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez_compressed(args.output / "depths.npz", **{
        f"{kind}_{budget}": frame
        for budget, frames in images.items()
        for kind, frame in zip(("reference", "retained", "early"), frames)
    })
    print(json.dumps(report))


if __name__ == "__main__":
    main()
