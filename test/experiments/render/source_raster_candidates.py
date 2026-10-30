"""Outer primitive support under an explicit raster/FP32 error model.

This selects original indices, never pixel colours or a unique first hit.
Unqualified projection/work retains the whole source. It is not GPU admission.
"""

import numpy as np

MAX_VERTICES = 262144
MAX_TRIANGLES = 131072
MAX_PAIRS = 4194304
MAX_CHUNK = 65536


def projection(positions, view_words, placement_words, raster):
    """Static litVertex order; finite FP32 normal arithmetic is an assumption."""
    view = view_words.view("<f4")
    placement = (
        placement_words[0, :16].view("<f4").reshape(4, 4, order="F").astype(float)
    )
    matrix = view[:16].reshape(4, 4, order="F").astype(float)
    shift = np.r_[view[48:51].astype(float), 0.0]
    operands = (positions, placement, matrix, shift)
    tiny, eps = float(np.finfo(np.float32).tiny), float(np.finfo(np.float32).eps)
    if any(
        not np.isfinite(a).all() or np.any((abs(a) > 0) & (abs(a) < tiny))
        for a in operands
    ):
        return None
    gamma = 7 * eps / (1 - 7 * eps)
    points = np.c_[positions.astype(float), np.ones(len(positions))]
    stable = points @ placement.T
    error = gamma * (abs(points) @ abs(placement).T) + 7 * tiny
    world = stable + shift
    error += eps * (abs(world) + error) + tiny
    clip = world @ matrix.T
    maximum = float(np.finfo(np.float32).max)
    if (
        np.any(abs(points) @ abs(placement).T > maximum)
        or np.any(abs(stable) + abs(shift) + error > maximum)
        or np.any((abs(world) + error) @ abs(matrix).T > maximum)
    ):
        return None
    error = (
        error @ abs(matrix).T
        + gamma * ((abs(world) + error) @ abs(matrix).T)
        + 7 * tiny
    )
    lower, upper = (
        np.nextafter(clip - error, -np.inf),
        np.nextafter(clip + error, np.inf),
    )
    if (
        not np.isfinite(lower).all()
        or not np.isfinite(upper).all()
        or np.any(abs(stable) > np.finfo(np.float32).max)
        or np.any(abs(world) > np.finfo(np.float32).max)
        or np.any(abs(clip) > np.finfo(np.float32).max)
        or np.any(lower[:, 3] <= 0)
        or np.any(lower[:, 2] <= 0)
        or np.any(upper[:, 2] >= lower[:, 3])
        or np.any(lower[:, :2] <= -lower[:, 3, None])
        or np.any(upper[:, :2] >= lower[:, 3, None])
    ):
        return None
    ratios = np.stack(
        [a[:, :2] / b[:, 3, None] for a in (lower, upper) for b in (lower, upper)]
    )
    lo, hi = np.nextafter(ratios, -np.inf).min(0), np.nextafter(ratios, np.inf).max(0)
    # All viewport centers are invariant under the native downward-y reflection.
    center = (clip[:, :2] / clip[:, 3, None] + 1) * raster / 2
    lo = np.nextafter((lo + 1) * raster / 2, -np.inf)
    hi = np.nextafter((hi + 1) * raster / 2, np.inf)
    radius = np.maximum(center - lo, hi - center)
    radius += 4 * eps * (abs(center) + raster) + tiny
    return center, np.nextafter(radius, np.inf)


def select(
    source,
    view_words,
    placement_words,
    raster,
    *,
    raster_guard_px,
    pairs_most=MAX_PAIRS,
    chunk_pairs=MAX_CHUNK,
):
    """Minkowski rectangle support; all viewport pixel centers are requested."""
    p, triangles = source["positions"], source["indices"]
    raster = np.asarray(raster)
    if (
        p.dtype != np.dtype("<f4")
        or p.ndim != 2
        or p.shape[1] != 3
        or not 1 <= len(p) <= MAX_VERTICES
        or not np.isfinite(p).all()
        or triangles.ndim != 2
        or triangles.shape[1] != 3
        or triangles.dtype.kind not in "iu"
        or not 1 <= len(triangles) <= MAX_TRIANGLES
        or triangles.min() < 0
        or triangles.max() >= len(p)
        or view_words.dtype != np.dtype("<u4")
        or view_words.shape != (56,)
        or placement_words.dtype != np.dtype("<u4")
        or placement_words.shape != (1, 36)
        or raster.shape != (2,)
        or raster.dtype.kind not in "iu"
        or np.any(raster <= 0)
        or np.any(raster > 65536)
        or type(raster_guard_px) not in (int, float)
        or not np.isfinite(raster_guard_px)
        or raster_guard_px < 0
        or type(pairs_most) is not int
        or not 0 <= pairs_most <= MAX_PAIRS
        or type(chunk_pairs) is not int
        or not 1 <= chunk_pairs <= MAX_CHUNK
    ):
        raise ValueError(
            "Bounded original geometry, actual upload layout and explicit raster guard required"
        )
    work = {
        "original_triangles": len(triangles),
        "vertices": len(p),
        "raster_guard_px": float(raster_guard_px),
        "raster": raster.tolist(),
        "bbox_pairs": 0,
        "tested_pairs": 0,
        "accepted_pairs": 0,
        "pair_chunks": 0,
        "max_chunk_pairs": 0,
        "qualified_gpu_raster": False,
    }

    def fallback(reason):
        return np.arange(len(triangles), dtype=np.uint32), {
            **work,
            "complete_model": False,
            "reason": reason,
            "original_group_retained": True,
        }

    projected = projection(p, view_words, placement_words, raster.astype(float))
    if projected is None:
        return fallback("clip_or_arithmetic_unknown")
    xy, vertex_error = projected
    vertex = xy[triangles]
    error = vertex_error[triangles].max(1) + raster_guard_px
    low = np.maximum(
        0, np.ceil(np.nextafter(vertex.min(1) - error - 0.5, -np.inf))
    ).astype(np.int64)
    high = np.minimum(
        raster - 1, np.floor(np.nextafter(vertex.max(1) + error - 0.5, np.inf))
    ).astype(np.int64)
    width = np.maximum(0, high - low + 1)
    count = width.prod(1)
    total = int(count.sum())
    work["bbox_pairs"] = total
    if total > pairs_most:
        return fallback("triangle_pixel_pair_budget")
    selected = np.zeros(len(triangles), bool)
    # Cumulative counts map bounded pair chunks without an all-pair expansion.
    ends = np.cumsum(count)
    starts = ends - count
    for begin in range(0, total, chunk_pairs):
        pair = np.arange(begin, min(total, begin + chunk_pairs))
        face = np.searchsorted(ends, pair, side="right")
        local = pair - starts[face]
        point = low[face] + np.c_[local % width[face, 0], local // width[face, 0]] + 0.5
        v = vertex[face]
        edge = np.roll(v, -1, axis=1) - v
        relative = point[:, None, :] - v
        area = (v[:, 1, 0] - v[:, 0, 0]) * (v[:, 2, 1] - v[:, 0, 1]) - (
            v[:, 1, 1] - v[:, 0, 1]
        ) * (v[:, 2, 0] - v[:, 0, 0])
        area_error = (
            16
            * np.finfo(float).eps
            * (
                abs((v[:, 1, 0] - v[:, 0, 0]) * (v[:, 2, 1] - v[:, 0, 1]))
                + abs((v[:, 1, 1] - v[:, 0, 1]) * (v[:, 2, 0] - v[:, 0, 0]))
            )
        )
        orientation = np.where(abs(area) <= area_error, 0.0, np.sign(area))
        value = (
            edge[:, :, 0] * relative[:, :, 1] - edge[:, :, 1] * relative[:, :, 0]
        ) * orientation[:, None]
        dilation = (
            abs(edge[:, :, 0]) * error[face, 1, None]
            + abs(edge[:, :, 1]) * error[face, 0, None]
        )
        # Directed outward slack covers the binary64 edge evaluation itself.
        slack = (
            16
            * np.finfo(float).eps
            * (
                abs(edge[:, :, 0] * relative[:, :, 1])
                + abs(edge[:, :, 1] * relative[:, :, 0])
                + dilation
            )
        )
        possible = (value >= -np.nextafter(dilation + slack, np.inf)).all(1)
        selected[face[possible]] = True
        work["tested_pairs"] += len(pair)
        work["accepted_pairs"] += int(possible.sum())
        work["pair_chunks"] += 1
        work["max_chunk_pairs"] = max(work["max_chunk_pairs"], len(pair))
    ids = np.flatnonzero(selected).astype(np.uint32)
    return ids, {
        **work,
        "complete_model": True,
        "reason": None,
        "original_group_retained": False,
        "selected_triangles": len(ids),
        "output_index_bytes": len(ids) * 12,
    }
