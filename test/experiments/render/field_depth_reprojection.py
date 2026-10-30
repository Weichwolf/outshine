"""Bounded point-ray evaluation of a discrete first-hit capture-cell model.

Each opaque input pixel represents its half-open pixel square at constant raw
depth. Empty cells are misses of THIS model, not proof of original visibility.
No depth interpolation, hidden-surface reconstruction or World admission occurs.
All matrices map the same field frame to clip coordinates (reverse-Z in [0,1]).
"""

from dataclasses import dataclass
from math import ceil, floor

import numpy as np
from depth_reprojection import transfer_matrix


@dataclass(frozen=True)
class Reprojected:
    rgba: np.ndarray
    reverse_z: np.ndarray
    evaluated: np.ndarray
    work: dict


def cell_segment(a, b, depth_range, dimensions):
    """Intersect one World point ray with the represented capture-depth slab."""
    ends = []
    for depth in depth_range:
        denominator = depth * b[3] - b[2]
        if denominator == 0:
            return None
        world_z = (a[2] - depth * a[3]) / denominator
        clip = a + world_z * b
        if not np.isfinite(clip).all() or clip[3] <= 0:
            return None
        with np.errstate(over="ignore", invalid="ignore"):
            ndc = clip[:2] / clip[3]
        if not np.isfinite(ndc).all():
            return None
        ends.append((ndc * [1, -1] + 1) * dimensions / 2)
    start, end = ends
    with np.errstate(over="ignore", invalid="ignore"):
        delta = end - start
    if not np.isfinite(delta).all():
        return None
    low, high = 0.0, 1.0
    for axis, limit in enumerate(dimensions):
        if delta[axis] == 0:
            if not 0 <= start[axis] < limit:
                return ()
        else:
            first, last = sorted(
                (-start[axis] / delta[axis], (limit - start[axis]) / delta[axis])
            )
            low, high = max(low, first), min(high, last)
    if high < low:
        return ()
    return start, delta, low, high


def crossed_cells(segment, dimensions, cells_most):
    """Enumerate grid crossings, bounded before expanding integer coordinates."""
    start, delta, low, high = segment
    start, delta = tuple(map(float, start)), tuple(map(float, delta))
    low, high = float(low), float(high)
    breaks = [low, high]
    ranges = []
    for axis, limit in enumerate(dimensions):
        first, last = sorted(
            (start[axis] + low * delta[axis], start[axis] + high * delta[axis])
        )
        begin, stop = max(0, ceil(first)), min(int(limit), floor(last)) + 1
        ranges.append((begin, stop))
    if 3 + sum(max(0, stop - begin) for begin, stop in ranges) > cells_most:
        return None
    for axis, (begin, stop) in enumerate(ranges):
        if delta[axis] != 0:
            breaks.extend(
                (edge - start[axis]) / delta[axis] for edge in range(begin, stop)
            )
    breaks = sorted({max(low, min(high, t)) for t in breaks})
    # Endpoints preserve the half-open cell on a grid edge or a tangent ray.
    values = breaks + [
        (a + b) / 2 for a, b in zip(breaks[:-1], breaks[1:], strict=True)
    ]
    cells = set()
    for t in values:
        x, y = (floor(start[k] + t * delta[k]) for k in (0, 1))
        if 0 <= x < dimensions[0] and 0 <= y < dimensions[1]:
            cells.add((x, y))
    return (
        np.asarray(sorted(cells), dtype=np.int64).reshape(-1, 2)
        if len(cells) <= cells_most
        else None
    )


def ray_hit(a, b, cells, rgba, depth, dimensions):
    x, y = cells.T
    d = depth[y, x].astype(float)
    denominator = d * b[3] - b[2]
    valid = (d > 0) & (denominator != 0)
    world_z = np.divide(a[2] - d * a[3], denominator, out=np.zeros_like(d), where=valid)
    clip = a + world_z[:, None] * b
    valid &= (world_z > 0) & (world_z <= 1) & (clip[:, 3] > 0)
    ndc = np.divide(
        clip[:, :2], clip[:, 3:], out=np.zeros((len(d), 2)), where=clip[:, 3:] != 0
    )
    pixel = (ndc * [1, -1] + 1) * dimensions / 2
    valid &= (
        np.isfinite(pixel).all(1) & (pixel >= cells).all(1) & (pixel < cells + 1).all(1)
    )
    if not valid.any():
        return np.zeros(4, dtype=rgba.dtype), 0.0
    nearest = int(np.argmax(np.where(valid, world_z, -np.inf)))
    return rgba[y[nearest], x[nearest]], world_z[nearest]


def reproject(
    rgba,
    depth,
    capture_from_field,
    world_from_field,
    pixels,
    raster,
    *,
    cells_most=512,
    steps_most=1048576,
    samples_most=65536,
    texels_most=65536,
):
    """Evaluate requested World sample positions, with bounded local failures.

    `evaluated` only means the reconstructed cell model was evaluated. It does
    not mean a newly visible original surface is absent. The caller must qualify
    approximation/lighting/coverage against originals before replacing them.
    Caller-owned arrays remain borrowed and unchanged for this synchronous call.
    """
    capacities = (cells_most, steps_most, samples_most, texels_most)
    if any(type(v) is not int or v < 1 for v in capacities):
        raise ValueError("Positive integer sample/cell/work capacities required")
    if (
        not isinstance(rgba, np.ndarray)
        or rgba.ndim != 3
        or rgba.shape[2] != 4
        or min(rgba.shape[:2]) < 1
        or np.prod(rgba.shape[:2]) > texels_most
        or rgba.dtype not in (np.dtype(np.float32), np.dtype(np.float64))
        or not isinstance(depth, np.ndarray)
        or depth.shape != rgba.shape[:2]
        or depth.dtype != rgba.dtype
        or not np.isfinite(rgba).all()
        or not np.isfinite(depth).all()
        or np.any(rgba < 0)
        or np.any((depth < 0) | (depth > 1))
        or not np.isin(rgba[:, :, 3], [0, 1]).all()
        or np.any((depth > 0) != (rgba[:, :, 3] == 1))
        or np.any(rgba[depth == 0] != 0)
    ):
        raise ValueError("Bounded coupled opaque raw C/coverage/depth required")
    pixels = np.asarray(pixels, dtype=float)
    if (
        pixels.ndim != 2
        or pixels.shape[1] != 2
        or len(pixels) > samples_most
        or not np.isfinite(pixels).all()
        or len(raster) != 2
        or any(type(v) is not int or v < 1 for v in raster)
        or np.any(pixels < 0)
        or np.any(pixels >= raster)
    ):
        raise ValueError("Bounded finite in-raster World sample coordinates required")
    try:
        transfer = np.linalg.inv(
            transfer_matrix(capture_from_field, world_from_field, np.zeros(3))
        )
    except np.linalg.LinAlgError as error:
        raise ValueError("Invertible World projection required") from error
    if not np.isfinite(transfer).all():
        raise ValueError("Finite capture/World transfer required")
    result = np.zeros((len(pixels), 4), dtype=rgba.dtype)
    z = np.zeros(len(pixels), dtype=np.float64)
    evaluated = np.zeros(len(pixels), dtype=bool)
    work = {"samples": len(pixels), "visited_cells": 0, "failed_samples": 0}
    hit_depth = depth[depth > 0]
    if not len(hit_depth):
        evaluated[:] = True
        return Reprojected(result, z, evaluated, work)
    depth_range = np.nextafter(
        [float(hit_depth.min()), float(hit_depth.max())], [-np.inf, np.inf]
    )
    dimensions = np.array(rgba.shape[1::-1])
    ndc = pixels / raster * 2 - 1
    ndc[:, 1] *= -1
    starts = np.c_[ndc, np.zeros(len(pixels)), np.ones(len(pixels))] @ transfer.T
    b = transfer[:, 2]
    for at, a in enumerate(starts):
        segment = cell_segment(a, b, depth_range, dimensions)
        if segment is None:
            continue
        if not len(segment):
            evaluated[at] = True
            continue
        allowance = min(cells_most, steps_most - work["visited_cells"])
        cells = crossed_cells(segment, dimensions, allowance) if allowance > 0 else None
        if cells is None:
            continue
        work["visited_cells"] += len(cells)
        evaluated[at] = True
        if len(cells):
            result[at], z[at] = ray_hit(a, b, cells, rgba, depth, dimensions)
    work["failed_samples"] = int(np.count_nonzero(~evaluated))
    return Reprojected(result, z, evaluated, work)
