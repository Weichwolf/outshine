"""Bounded all-surface column index; it retains complete original geometry.

This geometric reference has no shading/admission/IO policy. Cells hold original
triangle ordinals at every depth, not a sampled first-hit colour or invented ID.
"""

import time
from dataclasses import dataclass

import numpy as np
from field_depth_reprojection import cell_segment, crossed_cells
from triangle_pair_batches import MAX_PAIRS, MAX_TRIANGLES, TriangleBatches
from triangle_query_support import potential_samples


class ColumnLimit(ValueError):
    pass


def immutable(array, dtype):
    value = np.asarray(array, dtype=dtype, order="C")
    return np.frombuffer(value.tobytes(), dtype=dtype).reshape(value.shape)


@dataclass(frozen=True)
class TriangleColumns:
    positions: np.ndarray
    triangles: np.ndarray
    capture: np.ndarray
    size: int
    depth_range: tuple
    offsets: np.ndarray
    faces: np.ndarray
    bounds: np.ndarray

    @property
    def payload_bytes(self):
        return sum(
            v.nbytes
            for v in (
                self.positions,
                self.triangles,
                self.capture,
                self.offsets,
                self.faces,
                self.bounds,
            )
        )


def prepare(source, capture, size=64, *, references_most=1048576):
    """Own geometry; preflight every reference before expanding the CSR lists."""
    p, t, c = source["positions"], source["indices"], np.asarray(capture, float)
    if (
        type(size) is not int
        or not 2 <= size <= 128
        or type(references_most) is not int
        or not 1 <= references_most <= 4194304
        or p.ndim != 2
        or p.shape[1] != 3
        or p.dtype != np.float32
        or not 1 <= len(p) <= 262144
        or not np.isfinite(p).all()
        or t.ndim != 2
        or t.shape[1] != 3
        or t.dtype.kind not in "iu"
        or not 1 <= len(t) <= MAX_TRIANGLES
        or t.min() < 0
        or t.max() >= len(p)
        or c.shape != (4, 4)
        or not np.isfinite(c).all()
    ):
        raise ValueError("Bounded original Float32 geometry and capture required")
    clip = np.c_[p, np.ones(len(p))] @ c.T
    if np.any(clip[:, 3] <= 0) or not np.isfinite(clip).all():
        raise ValueError("Whole original must be in front of the capture")
    ndc = clip[:, :3] / clip[:, 3:]
    if np.any(abs(ndc[:, :2]) > 1) or np.any((ndc[:, 2] < 0) | (ndc[:, 2] > 1)):
        raise ValueError("Whole original must fit the declared capture volume")
    xy = (ndc[:, :2] * [1, -1] + 1) * size / 2
    bounds = xy[t]
    low = np.clip(np.floor(np.nextafter(bounds.min(1), -np.inf)), 0, size - 1).astype(
        int
    )
    high = np.clip(np.floor(np.nextafter(bounds.max(1), np.inf)), 0, size - 1).astype(
        int
    )
    width = high - low + 1
    count = width.prod(1)
    total = int(count.sum())
    if total > references_most:
        raise ColumnLimit("column_reference_budget; retain whole original group")
    face = np.repeat(np.arange(len(t), dtype=np.uint32), count)
    local = np.arange(total) - np.repeat(np.cumsum(count) - count, count)
    cell = (
        (low[face, 1] + local // width[face, 0]) * size
        + low[face, 0]
        + local % width[face, 0]
    )
    order = np.argsort(cell, kind="stable")
    offsets = np.r_[0, np.cumsum(np.bincount(cell, minlength=size * size))]
    return TriangleColumns(
        immutable(p, np.float32),
        immutable(t, np.uint32),
        immutable(c, np.float64),
        size,
        (float(ndc[:, 2].min()), float(ndc[:, 2].max())),
        immutable(offsets, np.uint32),
        immutable(face[order], np.uint32),
        immutable(np.stack((p.min(0), p.max(0))), np.float32),
    )


def world_rays(world, points, raster):
    clip = np.c_[
        points / raster * [2, -2] + [-1, 1], np.zeros(len(points)), np.ones(len(points))
    ]
    inverse = np.linalg.inv(world)
    far = clip @ inverse.T
    clip[:, 2] = 1
    near = clip @ inverse.T
    origin = near[:, :3] / near[:, 3:]
    direction = far[:, :3] / far[:, 3:] - origin
    direction /= np.linalg.norm(direction, axis=1)[:, None]
    if not np.isfinite(origin).all() or not np.isfinite(direction).all():
        raise ValueError("Finite bounded World rays required")
    return origin, direction


def nearest(positions, triangles, candidates, origin, direction):
    """Front-face Moller-Trumbore; no tolerance dilates original coverage."""
    a, b, c = positions[triangles[candidates]].astype(float).transpose(1, 0, 2)
    e1, e2 = b - a, c - a
    p = np.cross(direction, e2)
    determinant = np.einsum("ij,ij->i", e1, p)
    front = determinant > 1e-12
    reciprocal = np.divide(
        1.0, determinant, out=np.zeros_like(determinant), where=front
    )
    relative = origin - a
    u = np.einsum("ij,ij->i", relative, p) * reciprocal
    q = np.cross(relative, e1)
    v = q @ direction * reciprocal
    depth = np.einsum("ij,ij->i", e2, q) * reciprocal
    valid = front & (u >= 0) & (v >= 0) & (u + v <= 1) & (depth > 0)
    index = int(np.argmin(np.where(valid, depth, np.inf)))
    return (
        (float(depth[index]), int(candidates[index])) if valid.any() else (np.inf, -1)
    )


def query(
    field,
    world,
    points,
    raster,
    *,
    pairs_most=4194304,
    cells_most=512,
    time_limit_s=10,
    whole_support=True,
    pair_batch_size=4096,
):
    """Synchronous bounded reference. Unknown samples require the original group."""
    points, world, raster = (
        np.asarray(points, float),
        np.asarray(world, float),
        np.asarray(raster, float),
    )
    if (
        points.ndim != 2
        or points.shape[1] != 2
        or len(points) > 65536
        or not np.isfinite(points).all()
        or world.shape != (4, 4)
        or not np.isfinite(world).all()
        or raster.shape != (2,)
        or np.any(raster <= 0)
        or not np.isfinite(raster).all()
        or type(pairs_most) is not int
        or not 0 <= pairs_most <= 16777216
        or type(cells_most) is not int
        or not 1 <= cells_most <= 1024
        or not np.isfinite(time_limit_s)
        or time_limit_s < 0
        or type(whole_support) is not bool
        or type(pair_batch_size) is not int
        or not 0 <= pair_batch_size <= MAX_PAIRS
    ):
        raise ValueError("Bounded World samples and cooperative work limits required")
    began = time.monotonic()
    potential, bound_available = (
        potential_samples(field.bounds, world, points, raster)
        if whole_support
        else (np.ones(len(points), bool), False)
    )
    selected = range(len(points)) if potential.all() else np.flatnonzero(potential)
    samples = points[selected] if len(selected) != len(points) else points
    origins, directions = world_rays(world, samples, raster)
    transfer = field.capture @ np.linalg.inv(world)
    a = (
        np.c_[
            samples / raster * [2, -2] + [-1, 1],
            np.zeros(len(samples)),
            np.ones(len(samples)),
        ]
        @ transfer.T
    )
    b = transfer[:, 2]
    depth, owner, evaluated = (
        np.zeros(len(points)),
        np.full(len(points), -1, np.int64),
        ~potential,
    )
    visited = references = pairs = 0
    batch = None
    for row, i in enumerate(selected):
        if time.monotonic() - began > time_limit_s:
            break
        segment = cell_segment(a[row], b, field.depth_range, (field.size, field.size))
        if segment is None:
            continue
        if not segment:
            evaluated[i] = True
            continue
        cells = crossed_cells(segment, (field.size, field.size), cells_most)
        if cells is None:
            continue
        visited += len(cells)
        linear = cells[:, 1] * field.size + cells[:, 0]
        starts, ends = field.offsets[linear], field.offsets[linear + 1]
        required = int((ends - starts).sum(dtype=np.int64))
        if references + required > pairs_most:
            break
        references += required
        if required:
            candidates = np.unique(
                np.concatenate(
                    [
                        field.faces[int(lo) : int(hi)]
                        for lo, hi in zip(starts, ends, strict=True)
                    ]
                )
            )
            pairs += len(candidates)
            if pair_batch_size:
                if batch is None:
                    batch = TriangleBatches(
                        field.positions,
                        field.triangles,
                        origins,
                        directions,
                        pair_batch_size,
                    )
                batch.add(row, candidates)
                evaluated[i] = True
                continue
            distance, owner[i] = nearest(
                field.positions,
                field.triangles,
                candidates,
                origins[row],
                directions[row],
            )
            if np.isfinite(distance):
                hit = world @ np.r_[origins[row] + distance * directions[row], 1]
                z = hit[2] / hit[3]
                if hit[3] <= 0 or not 0 < z <= 1:
                    continue
                depth[i] = z
        evaluated[i] = True
    if batch is not None:
        batch.flush()
        for row in np.flatnonzero(batch.unknown):
            evaluated[selected[row]] = False
        for row in np.flatnonzero(np.isfinite(batch.distance) & ~batch.unknown):
            i = selected[row]
            owner[i] = batch.owner[row]
            hit = world @ np.r_[origins[row] + batch.distance[row] * directions[row], 1]
            z = hit[2] / hit[3]
            if hit[3] <= 0 or not 0 < z <= 1:
                evaluated[i] = False
                continue
            depth[i] = z
    return (
        depth,
        owner,
        evaluated,
        {
            "samples": len(points),
            "ray_rows": len(samples),
            "support_rejected_samples": int((~potential).sum()),
            "support_bounds_available": bound_available,
            "source_bound_corners": 8 if whole_support else 0,
            "visited_cells": visited,
            "triangle_references": references,
            "triangle_tests": pairs,
            "failed_samples": int((~evaluated).sum()),
            "direct_triangle_pairs": len(points) * len(field.triangles),
            "result_payload_bytes": depth.nbytes + owner.nbytes + evaluated.nbytes,
            "intersection_work": batch.work() if batch is not None else None,
        },
    )
