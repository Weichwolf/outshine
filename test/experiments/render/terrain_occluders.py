"""Test lower-envelope terrain occluders against complete cached heightfield rasterization."""

import argparse
import hashlib
import csv
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from current_visibility import box_bounds, occluded, project, raster
from terrain_grid_lod import page_frame, page_geometry, triangles, raster_view
from terrain_visibility import partition


def cross(a, b):
    return a[..., 0] * b[..., 1] - a[..., 1] * b[..., 0]


def lower_envelopes(points, side):
    points = np.asarray(points, dtype=float)
    if points.ndim != 3 or points.shape[1:] != (side * side, 3) or side < 2:
        raise ValueError("expected complete square heightfield patches")
    corners = points[:, [0, side - 1, side * side - 1, side * (side - 1)]]
    xy = corners[..., :2]
    edges = np.roll(xy, -1, axis=1) - xy
    turns = cross(edges, np.roll(edges, -1, axis=1))
    valid = np.isfinite(points).all(axis=(1, 2)) & (
        (turns > 0).all(axis=1) | (turns < 0).all(axis=1)
    )
    grid = np.arange(side * side).reshape(side, side)
    boundary = np.stack([grid[0], grid[:, -1], grid[-1, ::-1], grid[::-1, 0]])
    relative = points[:, boundary, :2] - xy[:, :, None]
    lengths = np.sum(edges * edges, axis=2)
    valid &= (lengths > 0).all(axis=1)
    lengths = np.maximum(lengths, np.finfo(float).tiny)
    along = np.einsum("nepi,nei->nep", relative, edges) / lengths[:, :, None]
    valid &= (
        np.abs(cross(relative, edges[:, :, None])) <= 1e-10 * lengths[:, :, None]
    ).all(axis=(1, 2))
    valid &= ((along >= -1e-10) & (along <= 1 + 1e-10)).all(axis=(1, 2))
    valid &= (np.diff(along, axis=2) >= 0).all(axis=(1, 2))
    faces = triangles(side, 1)
    fine = points[:, faces, :2]
    area = cross(fine[:, :, 1] - fine[:, :, 0], fine[:, :, 2] - fine[:, :, 0])
    valid &= (area > 0).all(axis=1) | (area < 0).all(axis=1)
    boundary_area = cross(xy[:, 1] - xy[:, 0], xy[:, 2] - xy[:, 0]) + cross(
        xy[:, 2] - xy[:, 0], xy[:, 3] - xy[:, 0]
    )
    valid &= np.isclose(
        np.abs(area).sum(axis=1), np.abs(boundary_area), rtol=1e-10, atol=1e-12
    )
    a, b = corners[:, 1] - corners[:, 0], corners[:, 3] - corners[:, 0]
    det = cross(a[:, :2], b[:, :2])
    valid &= np.abs(det) > 1e-12 * np.maximum(
        np.linalg.norm(a[:, :2], axis=1) * np.linalg.norm(b[:, :2], axis=1), 1
    )
    det = np.where(valid, det, 1)
    gradient = np.column_stack(
        (
            (a[:, 2] * b[:, 1] - b[:, 2] * a[:, 1]) / det,
            (a[:, 0] * b[:, 2] - b[:, 0] * a[:, 2]) / det,
        )
    )
    plane = corners[:, 0, 2, None] + np.einsum(
        "npi,ni->np", points[..., :2] - corners[:, 0, None, :2], gradient
    )
    residual = points[..., 2] - plane
    margin = (
        64
        * np.finfo(float).eps
        * np.maximum(np.abs(points[..., 2]).max(axis=1) + np.abs(plane).max(axis=1), 1)
    )
    lower = residual.min(axis=1) - margin
    proxy = corners.copy()
    proxy[..., 2] = (
        corners[:, 0, 2, None]
        + np.einsum("npi,ni->np", xy - xy[:, 0, None], gradient)
        + lower[:, None]
    )
    return proxy, valid, plane + lower[:, None], residual.max(axis=1) - lower + margin


def bracketed_proxies(proxy, gaps, eye):
    count = len(proxy)
    a, b = proxy[:, 1] - proxy[:, 0], proxy[:, 3] - proxy[:, 0]
    det = cross(a[:, :2], b[:, :2])
    det = np.where(np.abs(det) > 1e-20, det, 1)
    gradient = np.column_stack(
        (
            (a[:, 2] * b[:, 1] - b[:, 2] * a[:, 1]) / det,
            (a[:, 0] * b[:, 2] - b[:, 0] * a[:, 2]) / det,
        )
    )
    above = (
        eye[2]
        - proxy[:, 0, 2]
        - np.einsum("ni,ni->n", eye[:2] - proxy[:, 0, :2], gradient)
    )
    valid = above > gaps
    fraction = 1 - gaps / np.where(valid, above, 1)
    points = np.zeros((count, 8, 3))
    points[:, :4] = proxy
    sizes = np.where(valid, 4, 0)
    orientation = np.sign(cross(a[:, :2], b[:, :2]))
    indices = np.arange(8)[None]
    for edge in range(4):
        following = np.take_along_axis(
            points, ((indices + 1) % np.maximum(sizes[:, None], 1))[..., None], axis=1
        )
        direction = proxy[:, (edge + 1) % 4, :2] - proxy[:, edge, :2]
        upper = eye[:2] + fraction[:, None, None] * (points[..., :2] - eye[:2])
        values = orientation[:, None] * cross(
            direction[:, None], upper - proxy[:, edge, None, :2]
        )
        after = np.take_along_axis(
            values, (indices + 1) % np.maximum(sizes[:, None], 1), axis=1
        )
        active = indices < sizes[:, None]
        inside = (values >= 0) & active
        crossing = ((values >= 0) != (after >= 0)) & active
        denominator = np.where(crossing, values - after, 1)
        intersection = points + (values / denominator)[..., None] * (following - points)
        emitted = np.stack((inside, crossing), axis=2).reshape(count, 16)
        candidates = np.stack((points, intersection), axis=2).reshape(count, 16, 3)
        slots = emitted.cumsum(axis=1) - 1
        sizes = emitted.sum(axis=1)
        if np.any(sizes > 8):
            raise ValueError("convex quad intersection exceeds its eight-vertex bound")
        points = np.zeros_like(points)
        owners = np.broadcast_to(np.arange(count)[:, None], emitted.shape)
        points[owners[emitted], slots[emitted]] = candidates[emitted]
    return points, sizes


def compare(pages, scenario, width, height, cells, yaw, levels):
    if min(width, height, levels) <= 0:
        raise ValueError("image dimensions and pyramid levels must be positive")
    started = time.perf_counter()
    fingerprint = hashlib.sha256((pages / "pages.csv").read_bytes())
    with (pages / "pages.csv").open() as source:
        rows = list(csv.DictReader(source))
    view = raster_view(scenario, rows, pages, width, height, yaw)
    faces, nodes = partition(cells)
    baseline = triangles(33, 1)
    full = np.zeros((height, width))
    proxy_depth = full.copy()
    owner = np.full((height, width), -1, dtype=int)
    bounds, admitted = [], 0
    for page, row in enumerate(rows):
        _, local, _, world, stamp = page_geometry(row, pages)
        fingerprint.update(stamp)
        projected, z = project(world, view)
        full = np.maximum(full, raster(projected, z, baseline, width, height))
        points = world[nodes]
        bounds.append((points.min(axis=1), points.max(axis=1)))
        proxy, valid, lower, gaps = lower_envelopes(local[nodes], cells + 1)
        assert np.all(lower[valid] <= local[nodes][valid, :, 2] + 1e-9)
        centre, axes, _ = page_frame(row)
        clipped, sizes = bracketed_proxies(proxy, gaps, (view[0] - centre) @ axes.T)
        valid &= (z[nodes] > 0.1).all(axis=1)
        sizes = np.where(valid, sizes, 0)
        projected_proxy, proxy_z = project(
            (clipped @ axes + centre).reshape(-1, 3), view
        )
        fan = np.array([[0, i + 1, i] for i in range(1, 7)])
        ix = fan[None] + np.arange(len(nodes))[:, None, None] * 8
        used = np.arange(1, 7)[None] + 1 < sizes[:, None]
        admitted += int(used.sum())
        candidate = raster(
            projected_proxy, proxy_z, ix[used].reshape(-1, 3), width, height
        )
        owner[candidate > proxy_depth] = page
        proxy_depth = np.maximum(proxy_depth, candidate)
    bounds = tuple(np.concatenate([b[k] for b in bounds]) for k in (0, 1))
    lo, hi, nearest, safe, visible, _ = box_bounds(bounds, view)
    hidden = occluded(lo - 1, hi + 1, nearest, safe, proxy_depth, levels) & visible
    selected = (visible & ~hidden).reshape(len(rows), len(nodes))
    retained = np.zeros_like(full)
    for row, keep in zip(rows, selected):
        if not keep.any():
            continue
        _, _, _, world, _ = page_geometry(row, pages)
        projected, z = project(world, view)
        retained = np.maximum(
            retained, raster(projected, z, faces[keep].reshape(-1, 3), width, height)
        )
    difference = np.abs(full - retained)
    report = dict(
        pages=len(rows),
        input_sha256=fingerprint.hexdigest(),
        scenario_sha256=hashlib.sha256(scenario.read_bytes()).hexdigest(),
        width=width,
        height=height,
        cells=cells,
        yaw=yaw,
        levels=levels,
        proxy_triangles=admitted,
        frustum_triangles=int(visible.sum()) * cells * cells * 2,
        hidden_triangles=int(hidden.sum()) * cells * cells * 2,
        changed_pixels=int(np.count_nonzero(difference > 1e-12)),
        max_depth_difference=float(difference.max()),
        proxy_in_front_pixels=int(np.count_nonzero(proxy_depth > full + 1e-12)),
        unsafe_proxy_samples=[
            dict(
                x=int(x),
                y=int(y),
                page=rows[owner[y, x]]["page"],
                reference=float(full[y, x]),
                proxy=float(proxy_depth[y, x]),
            )
            for y, x in np.argwhere(proxy_depth > full + 1e-12)[:16]
        ],
        seconds=time.perf_counter() - started,
        scope="Ideal double-precision opaque heightfield, same-patch lower/upper ray brackets; no MSAA, shader rounding, material, skirts or native cost claim.",
    )
    return report, full, proxy_depth, retained


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pages", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=180)
    parser.add_argument("--cells", type=int, choices=[4, 8, 16, 32], default=8)
    parser.add_argument("--yaw", type=float, default=0)
    parser.add_argument("--levels", type=int, default=4)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("output must be in system temp")
    args.output.mkdir(parents=True, exist_ok=True)
    report, full, proxy, retained = compare(
        args.pages,
        args.scenario,
        args.width,
        args.height,
        args.cells,
        args.yaw,
        args.levels,
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez_compressed(
        args.output / "depths.npz", reference=full, occluders=proxy, retained=retained
    )
    print(json.dumps(report))


if __name__ == "__main__":
    main()
