"""Compare current-view occlusion against full rasterization of cached opaque geometry."""

import argparse
import json
import time
from pathlib import Path

import numpy as np


def pyramid(depth, levels=4):
    result = []
    for _ in range(levels):
        h, w = depth.shape
        padded = np.zeros((h + h % 2, w + w % 2), dtype=depth.dtype)
        padded[:h, :w] = depth
        depth = padded.reshape((h + 1) // 2, 2, (w + 1) // 2, 2).min(axis=(1, 3))
        result.append(depth)
        if depth.shape == (1, 1):
            break
    return result


def camera(scene, vertices, width, height, yaw):
    anchor = np.array(scene["anchor_ecef"], dtype=np.float64)
    eye = np.array(scene["eye_ecef"], dtype=np.float64) - anchor
    up = anchor / np.linalg.norm(anchor)
    toward = np.mean(vertices, axis=0) - eye
    toward -= np.dot(toward, up) * up
    toward /= np.linalg.norm(toward)
    side = np.cross(toward, up)
    forward = np.cos(yaw) * toward + np.sin(yaw) * side - 0.2 * up
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    upward = np.cross(right, forward)
    basis = np.stack((right, upward, forward))
    return eye, basis, scene["focal_px"] * height / 720, width, height


def project(points, view):
    eye, basis, focal, width, height = view
    local = np.einsum("ni,ji->nj", points - eye, basis)
    safe_z = np.where(np.abs(local[:, 2]) > 1e-12, local[:, 2], 1e-12)
    return np.column_stack(
        (
            width / 2 + focal * local[:, 0] / safe_z,
            height / 2 - focal * local[:, 1] / safe_z,
            0.1 / safe_z,
        )
    ), local[:, 2]


def bounds(spheres, view):
    eye, basis, focal, width, height = view
    centres = np.einsum("ni,ji->nj", spheres[:, :3] - eye, basis)
    radius = spheres[:, 3]
    near = centres[:, 2] - radius
    safe_near = np.maximum(near, 1e-12)
    far = centres[:, 2] + radius
    safe_far = np.maximum(far, safe_near)
    intervals = []
    for axis in (0, 1):
        low, high = centres[:, axis] - radius, centres[:, axis] + radius
        options = np.stack(
            (low / safe_near, low / safe_far, high / safe_near, high / safe_far)
        )
        intervals.append((options.min(axis=0), options.max(axis=0)))
    lo = np.column_stack(
        (width / 2 + focal * intervals[0][0], height / 2 - focal * intervals[1][1])
    )
    hi = np.column_stack(
        (width / 2 + focal * intervals[0][1], height / 2 - focal * intervals[1][0])
    )
    visible = (
        (hi[:, 0] >= 0)
        & (hi[:, 1] >= 0)
        & (lo[:, 0] < width)
        & (lo[:, 1] < height)
        & (far > 0.1)
    )
    safe = near > 0.1
    visible |= ~safe
    visible &= far > 0.1
    return lo, hi, 0.1 / safe_near, safe, visible, near


def mesh_boxes(vertices, triangles, owners, count):
    low = np.full((count, 3), np.inf)
    high = np.full((count, 3), -np.inf)
    expanded = np.repeat(owners, 3)
    points = vertices[triangles.ravel()]
    np.minimum.at(low, expanded, points)
    np.maximum.at(high, expanded, points)
    return low, high


def box_bounds(boxes, view):
    low, high = boxes
    corners = np.array([[x, y, z] for x in (0, 1) for y in (0, 1) for z in (0, 1)])
    points = low[:, None, :] + corners[None, :, :] * (high - low)[:, None, :]
    projected, z = project(points.reshape(-1, 3), view)
    projected = projected.reshape(-1, 8, 3)
    z = z.reshape(-1, 8)
    lo = projected[:, :, :2].min(axis=1)
    hi = projected[:, :, :2].max(axis=1)
    safe = z.min(axis=1) > 0.1
    visible = (
        (hi[:, 0] >= 0) & (hi[:, 1] >= 0) & (lo[:, 0] < view[3]) & (lo[:, 1] < view[4])
    ) | ~safe
    visible &= z.max(axis=1) > 0.1
    nearest = 0.1 / np.maximum(z.min(axis=1), 1e-12)
    return lo, hi, nearest, safe, visible, z.min(axis=1)


def occluded(
    lo, hi, nearest, safe, depth, level_count=4, absolute_bias=1e-6, relative_bias=0.0
):
    height, width = depth.shape
    lo = np.clip(lo, (0, 0), (width, height))
    hi = np.clip(hi, (0, 0), (width, height))
    levels = pyramid(depth, level_count)
    spans = np.maximum(hi - lo, 0).max(axis=1) / 2
    chosen = np.minimum(
        np.maximum(np.ceil(np.log2(np.maximum(spans / 2, 1))), 0), len(levels) - 1
    ).astype(int)
    hidden = np.zeros(len(lo), dtype=bool)
    for level, image in enumerate(levels):
        selected = np.flatnonzero(safe & (chosen == level))
        if not len(selected):
            continue
        step = 2 << level
        h, w = image.shape
        start = np.clip(np.floor(lo[selected] / step), 0, (w - 1, h - 1)).astype(int)
        end = np.clip(np.floor(hi[selected] / step), 0, (w - 1, h - 1)).astype(int)
        tested = (end - start).max(axis=1) < 4
        farthest = np.ones(len(selected))
        for dy in range(4):
            for dx in range(4):
                p = start + (dx, dy)
                active = (p <= end).all(axis=1)
                value = image[np.minimum(p[:, 1], h - 1), np.minimum(p[:, 0], w - 1)]
                farthest = np.minimum(farthest, np.where(active, value, 1))
        hidden[selected] = tested & (
            nearest[selected]
            + absolute_bias
            + np.abs(nearest[selected]) * relative_bias
            < farthest
        )
    return hidden


def raster(projected, forward_z, triangles, width, height):
    depth = np.zeros((height, width))
    if not len(triangles):
        return depth
    p = projected[triangles]
    front = (forward_z[triangles] > 0.1).all(axis=1)
    den = (p[:, 1, 1] - p[:, 2, 1]) * (p[:, 0, 0] - p[:, 2, 0]) + (
        p[:, 2, 0] - p[:, 1, 0]
    ) * (p[:, 0, 1] - p[:, 2, 1])
    front &= den < -1e-12
    low = np.ceil(
        np.clip(p[:, :, :2].min(axis=1) - 0.5, (-1, -1), (width, height))
    ).astype(int)
    high = np.floor(
        np.clip(p[:, :, :2].max(axis=1) - 0.5, (-1, -1), (width, height))
    ).astype(int)
    low = np.maximum(low, 0)
    high = np.minimum(high, (width - 1, height - 1))
    front &= (high >= low).all(axis=1)
    p, low, high, den = p[front], low[front], high[front], den[front]
    sizes = high - low + 1
    bins = np.ceil(np.log2(sizes)).astype(int)
    for bx, by in np.unique(bins, axis=0):
        select = np.flatnonzero((bins == (bx, by)).all(axis=1))
        bw, bh = 1 << int(bx), 1 << int(by)
        yy, xx = np.indices((bh, bw))
        dx, dy = xx.ravel(), yy.ravel()
        batch = max(1, 262144 // (bw * bh))
        for first in range(0, len(select), batch):
            ids = select[first : first + batch]
            q = p[ids]
            x = low[ids, 0, None] + dx
            y = low[ids, 1, None] + dy
            w0 = (
                (q[:, 1, 1, None] - q[:, 2, 1, None]) * (x + 0.5 - q[:, 2, 0, None])
                + (q[:, 2, 0, None] - q[:, 1, 0, None]) * (y + 0.5 - q[:, 2, 1, None])
            ) / den[ids, None]
            w1 = (
                (q[:, 2, 1, None] - q[:, 0, 1, None]) * (x + 0.5 - q[:, 2, 0, None])
                + (q[:, 0, 0, None] - q[:, 2, 0, None]) * (y + 0.5 - q[:, 2, 1, None])
            ) / den[ids, None]
            w2 = 1 - w0 - w1
            covered = (
                (w0 >= 0)
                & (w1 >= 0)
                & (w2 >= 0)
                & (x <= high[ids, 0, None])
                & (y <= high[ids, 1, None])
            )
            values = (
                w0 * q[:, 0, 2, None] + w1 * q[:, 1, 2, None] + w2 * q[:, 2, 2, None]
            )
            np.maximum.at(depth.ravel(), (y * width + x)[covered], values[covered])
    return depth


def load(directory):
    scene = json.loads((directory / "scene.json").read_text())
    vertices, triangles, spheres, owners = [], [], [], []
    vertex_offset = 0
    cluster_offset = 0
    for name in ("walls", "roofs"):
        v = np.fromfile(directory / f"{name}.positions.f32", dtype="<f4").reshape(-1, 3)
        ix = np.fromfile(directory / f"{name}.indices.u32", dtype="<u4").reshape(-1, 3)
        clusters = np.atleast_1d(
            np.genfromtxt(directory / f"{name}.clusters.csv", delimiter=",", names=True)
        )
        if not np.isfinite(v).all() or (len(ix) and ix.max() >= len(v)):
            raise ValueError("invalid mesh positions or indices")
        if np.any(clusters["level"] != 0) or np.any(clusters["error"] != 0):
            raise ValueError("the reference requires already selected leaf geometry")
        owner = np.full(len(ix), -1, dtype=int)
        for index, cluster in enumerate(clusters):
            first, count = int(cluster["first"]), int(cluster["count"])
            if (
                first < 0
                or count <= 0
                or first + count > ix.size
                or first % 3
                or count % 3
                or np.any(owner[first // 3 : (first + count) // 3] >= 0)
            ):
                raise ValueError("invalid or overlapping cluster index runs")
            owner[first // 3 : (first + count) // 3] = cluster_offset + index
        if np.any(owner < 0):
            raise ValueError("cluster runs omit triangles")
        vertices.append(v)
        triangles.append(ix + vertex_offset)
        owners.append(owner)
        spheres.append(
            np.column_stack([clusters[x] for x in ("x", "y", "z", "radius")])
        )
        vertex_offset += len(v)
        cluster_offset += len(clusters)
    return (
        scene,
        np.concatenate(vertices),
        np.concatenate(triangles),
        np.concatenate(spheres),
        np.concatenate(owners),
    )


def subdivide(owners, limit):
    counts = np.bincount(owners)
    first = np.cumsum(counts) - counts
    groups = (counts + limit - 1) // limit
    offsets = np.cumsum(groups) - groups
    return offsets[owners] + (np.arange(len(owners)) - first[owners]) // limit


def experiment(
    directory,
    width,
    height,
    seed_count,
    yaw=0,
    bound_mode="sphere",
    levels=4,
    cluster_triangles=None,
):
    if width <= 0 or height <= 0 or seed_count < 0 or levels <= 0:
        raise ValueError(
            "dimensions and level count must be positive, seed count nonnegative"
        )
    if cluster_triangles is not None and cluster_triangles <= 0:
        raise ValueError("cluster triangle limit must be positive")
    started = time.perf_counter()
    scene, vertices, triangles, spheres, owners = load(directory)
    if cluster_triangles is not None:
        owners = subdivide(owners, cluster_triangles)
        boxes = mesh_boxes(vertices, triangles, owners, int(owners.max()) + 1)
        centre = (boxes[0] + boxes[1]) / 2
        spheres = np.column_stack((centre, np.linalg.norm(boxes[1] - centre, axis=1)))
    else:
        boxes = (
            mesh_boxes(vertices, triangles, owners, len(spheres))
            if bound_mode == "aabb"
            else None
        )
    prepare_seconds = time.perf_counter() - started
    view = camera(scene, vertices, width, height, np.deg2rad(yaw))
    projected, z = project(vertices, view)
    lo, hi, nearest, safe, visible, distance = bounds(spheres, view)
    if bound_mode == "aabb":
        lo, hi, nearest, safe, visible, distance = box_bounds(boxes, view)
    pixels = np.maximum(np.minimum(hi, (width, height)) - np.maximum(lo, 0), 0).prod(
        axis=1
    )
    counts = np.bincount(owners, minlength=len(spheres))
    score = pixels / np.maximum(counts, 1)
    candidates = np.flatnonzero(visible & safe & (distance > 1))
    chosen = (
        candidates[np.argsort(score[candidates])[-seed_count:]] if seed_count else []
    )
    seed = np.zeros(len(spheres), dtype=bool)
    seed[chosen] = True
    source_triangles = visible[owners]
    started = time.perf_counter()
    seed_depth = raster(projected, z, triangles[seed[owners]], width, height)
    seed_seconds = time.perf_counter() - started
    started = time.perf_counter()
    hidden = occluded(lo, hi, nearest, safe, seed_depth, levels) & visible & ~seed
    cull_seconds = time.perf_counter() - started
    retained = source_triangles & ~hidden[owners]
    started = time.perf_counter()
    reference = raster(projected, z, triangles, width, height)
    result = raster(projected, z, triangles[retained], width, height)
    oracle_hidden = occluded(lo, hi, nearest, safe, reference, levels) & visible & ~seed
    oracle_seconds = time.perf_counter() - started
    differences = np.abs(reference - result)
    report = {
        "bounds": bound_mode,
        "levels": levels,
        "cluster_triangles": cluster_triangles,
        "asset": scene["asset"],
        "width": width,
        "height": height,
        "yaw": yaw,
        "input_triangles": len(triangles),
        "full_depth_hidden_triangles": int(
            (source_triangles & oracle_hidden[owners]).sum()
        ),
        "prepare_seconds": prepare_seconds,
        "clusters": len(spheres),
        "frustum_clusters": int(visible.sum()),
        "seed_clusters": int(seed.sum()),
        "hidden_clusters": int(hidden.sum()),
        "source_triangles": int(source_triangles.sum()),
        "seed_triangles": int(seed[owners].sum()),
        "retained_triangles": int(retained.sum()),
        "depth_difference_max": float(differences.max()),
        "changed_pixels": int(np.count_nonzero(differences > 1e-12)),
        "seed_seconds": seed_seconds,
        "cull_seconds": cull_seconds,
        "oracle_seconds": oracle_seconds,
    }
    return report, seed_depth, reference, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=180)
    parser.add_argument("--seeds", type=int, default=300)
    parser.add_argument("--yaw", type=float, default=0)
    parser.add_argument("--bounds", choices=("sphere", "aabb"), default="sphere")
    parser.add_argument("--levels", type=int, default=4)
    parser.add_argument("--cluster-triangles", type=int)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report, seed, full, culled = experiment(
        args.input,
        args.width,
        args.height,
        args.seeds,
        args.yaw,
        args.bounds,
        args.levels,
        args.cluster_triangles,
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez_compressed(
        args.output / "depths.npz", seed=seed, reference=full, culled=culled
    )
    print(json.dumps(report))


if __name__ == "__main__":
    main()
