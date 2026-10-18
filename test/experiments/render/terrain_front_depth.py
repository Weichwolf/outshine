"""Compare exact near-terrain occluders with a complete unchanged terrain raster."""

import argparse
import hashlib
import csv
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from current_visibility import box_bounds, occluded, project, raster
from terrain_grid_lod import page_geometry, triangles, raster_view, page_frame
from terrain_visibility import partition


def compare(
    pages,
    scenario,
    width,
    height,
    yaw,
    budgets,
    levels,
    priority="distance",
    bounds_kind="world",
    depth_bias="absolute",
):
    if min(width, height, levels, *budgets) <= 0:
        raise ValueError("dimensions, levels and occluder counts must be positive")
    started = time.perf_counter()
    fingerprint = hashlib.sha256((pages / "pages.csv").read_bytes())
    with (pages / "pages.csv").open() as source:
        rows = list(csv.DictReader(source))
    view = raster_view(scenario, rows, pages, width, height, yaw)
    faces, nodes = partition(8)
    baseline = triangles(33, 1)
    full = np.zeros((height, width))
    bounds, distance, limits = [], [], []
    for row in rows:
        _, local, _, world, stamp = page_geometry(row, pages)
        fingerprint.update(stamp)
        projected, z = project(world, view)
        full = np.maximum(full, raster(projected, z, baseline, width, height))
        points = world[nodes]
        bounds.append((points.min(axis=1), points.max(axis=1)))
        if bounds_kind != "world":
            if bounds_kind == "obb":
                local_points = local[nodes]
                low, high = local_points.min(axis=1), local_points.max(axis=1)
                corners = np.array(
                    [[x, y, z] for x in (0, 1) for y in (0, 1) for z in (0, 1)]
                )
                centre, axes, _ = page_frame(row)
                world_corners = (
                    low[:, None] + corners[None] * (high - low)[:, None]
                ) @ axes + centre
                pp, zz = project(world_corners.reshape(-1, 3), view)
                pp, zz = pp.reshape(len(nodes), 8, 3), zz.reshape(len(nodes), 8)
            else:
                pp, zz = projected[nodes], z[nodes]
            a, b = pp[..., :2].min(axis=1), pp[..., :2].max(axis=1)
            near = zz.min(axis=1)
            safe = near > 0.1
            visible = (
                (b[:, 0] >= 0) & (b[:, 1] >= 0) & (a[:, 0] < width) & (a[:, 1] < height)
            ) | ~safe
            visible &= zz.max(axis=1) > 0.1
            limits.append((a, b, 0.1 / np.maximum(near, 1e-12), safe, visible, near))
        delta = np.maximum(
            np.maximum(world.min(axis=0) - view[0], view[0] - world.max(axis=0)), 0
        )
        distance.append(np.linalg.norm(delta))
    boxes = tuple(np.concatenate([b[k] for b in bounds]) for k in (0, 1))
    lo, hi, nearest, safe, visible, _ = (
        box_bounds(boxes, view)
        if bounds_kind == "world"
        else tuple(np.concatenate([b[k] for b in limits]) for k in range(6))
    )
    visible = visible.reshape(len(rows), len(nodes))
    area = np.prod(
        np.maximum(np.minimum(hi, (width, height)) - np.maximum(lo, 0), 0), axis=1
    )
    area = np.where(safe, area, width * height).reshape(visible.shape)
    area = np.where(visible, area, 0).sum(axis=1)
    order = sorted(
        np.flatnonzero(visible.any(axis=1)),
        key=lambda i: (
            (-area[i], distance[i]) if priority == "coverage" else (distance[i],)
        ),
    )
    depth = np.zeros_like(full)
    near = np.zeros(len(rows), dtype=bool)
    results, selected = [], []
    for count in sorted(set(budgets)):
        for page in order[:count]:
            if near[page]:
                continue
            near[page] = True
            _, _, _, world, _ = page_geometry(rows[page], pages)
            projected, z = project(world, view)
            depth = np.maximum(depth, raster(projected, z, baseline, width, height))
        hidden = occluded(
            lo - 1,
            hi + 1,
            nearest,
            safe,
            depth,
            levels,
            absolute_bias=1e-6 if depth_bias == "absolute" else 0.0,
            relative_bias=1e-6 if depth_bias == "relative" else 0.0,
        ).reshape(visible.shape)
        keep = visible & ~hidden & ~near[:, None]
        selected.append((keep, depth.copy()))
        early = int(near.sum()) * len(baseline)
        late = int(keep.sum()) * 128
        results.append(
            dict(
                near_pages=int(near.sum()),
                early_triangles=early,
                late_triangles=late,
                total_triangles=early + late,
            )
        )
    for row, flags in zip(rows, zip(*(k for k, _ in selected))):
        if not any(k.any() for k in flags):
            continue
        _, _, _, world, _ = page_geometry(row, pages)
        projected, z = project(world, view)
        for k, (_, retained) in zip(flags, selected):
            if k.any():
                retained[:] = np.maximum(
                    retained,
                    raster(projected, z, faces[k].reshape(-1, 3), width, height),
                )
    for result, (_, retained) in zip(results, selected):
        difference = np.abs(full - retained)
        result.update(
            changed_pixels=int(np.count_nonzero(difference > 1e-12)),
            max_depth_difference=float(difference.max()),
        )
    return (
        dict(
            pages=len(rows),
            input_sha256=fingerprint.hexdigest(),
            scenario_sha256=hashlib.sha256(scenario.read_bytes()).hexdigest(),
            width=width,
            height=height,
            yaw=yaw,
            levels=levels,
            priority=priority,
            bounds_kind=bounds_kind,
            depth_bias=depth_bias,
            baseline_frustum_triangles=int(visible.sum()) * 128,
            cases=results,
            seconds=time.perf_counter() - started,
            scope="Exact opaque cached interiors, pixel-centre CPU model. Early pages are drawn once; no geometry/normal approximation. Skirts, MSAA, shader rounding, renderer scheduling and native cost remain unproved.",
        ),
        full,
        selected[-1][1],
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pages", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=180)
    parser.add_argument("--yaw", type=float, default=0)
    parser.add_argument("--levels", type=int, default=4)
    parser.add_argument(
        "--depth-bias", choices=["absolute", "relative"], default="absolute"
    )
    parser.add_argument(
        "--bounds", choices=["world", "obb", "surface"], default="world"
    )
    parser.add_argument(
        "--priority", choices=["distance", "coverage"], default="distance"
    )
    parser.add_argument("--near-pages", type=int, nargs="+", default=[16, 64, 256])
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("output must be in system temp")
    args.output.mkdir(parents=True, exist_ok=True)
    report, full, retained = compare(
        args.pages,
        args.scenario,
        args.width,
        args.height,
        args.yaw,
        args.near_pages,
        args.levels,
        args.priority,
        args.bounds,
        args.depth_bias,
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez_compressed(args.output / "depths.npz", reference=full, retained=retained)
    print(json.dumps(report))


if __name__ == "__main__":
    main()
