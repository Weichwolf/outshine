"""Coarse static boxes enclosing the existing conditional FP32 support model."""

import numpy as np

EPS64 = np.finfo(float).eps
EPS32 = float(np.finfo(np.float32).eps)
TINY32 = float(np.finfo(np.float32).tiny)


def frame(view_words, placements):
    view = view_words.view("<f4")
    matrix = view[:16].reshape(4, 4, order="F").astype(float)
    placement = placements[0, :16].view("<f4").reshape(4, 4, order="F").astype(float)
    shift = np.r_[view[48:51].astype(float), 0.0]
    if any(
        not np.isfinite(a).all() or np.any((abs(a) > 0) & (abs(a) < TINY32))
        for a in (matrix, placement, shift)
    ):
        return None
    return placement, shift, matrix


def clip_boxes(bounds, operands):
    """Interval affine stages, including generous binary64 evaluation guards."""
    placement, shift, matrix = operands
    lower = np.c_[bounds[:, 0].astype(float), np.ones(len(bounds))]
    upper = np.c_[bounds[:, 1].astype(float), np.ones(len(bounds))]
    for transform, translation in ((placement, shift), (matrix, np.zeros(4))):
        center = (lower + upper) / 2
        extent = np.nextafter(np.maximum(center - lower, upper - center), np.inf)
        mid = center @ transform.T
        radius = extent @ abs(transform).T
        magnitude = abs(center) @ abs(transform).T + radius
        error = magnitude * (64 * EPS64)
        lo, hi = mid - radius - error, mid + radius + error
        lower = np.nextafter(np.nextafter(lo, -np.inf) + translation, -np.inf)
        upper = np.nextafter(np.nextafter(hi, np.inf) + translation, np.inf)
    return lower, upper


def upper(value):
    return np.nextafter(value * (1 + 32 * EPS64), np.inf)


def clip_envelope(bounds, operands):
    """Unprojected root interval plus the shared conditional FP32 error bound."""
    placement, shift, matrix = operands
    absolute = np.r_[np.max(abs(bounds.astype(float)), axis=0), 1.0]
    gamma = 7 * EPS32 / (1 - 7 * EPS32)
    stable_norm = upper(absolute @ abs(placement).T)
    stable_error = upper(gamma * stable_norm + 7 * TINY32)
    world_norm = upper(stable_norm + abs(shift))
    world_error = upper(
        stable_error + EPS32 * upper(world_norm + stable_error) + TINY32
    )
    clip_norm = upper(upper(world_norm + world_error) @ abs(matrix).T)
    error = 2 * upper(
        upper(world_error @ abs(matrix).T) + gamma * clip_norm + 7 * TINY32
    )
    lo, hi = clip_boxes(bounds[None], operands)
    lo, hi = lo[0], hi[0]
    if (
        not np.isfinite(error).all()
        or not np.isfinite(lo).all()
        or not np.isfinite(hi).all()
        or np.any(stable_norm > np.finfo(np.float32).max)
        or np.any(upper(world_norm + world_error) > np.finfo(np.float32).max)
        or np.any(clip_norm > np.finfo(np.float32).max)
    ):
        return None
    return lo, hi, error


def support_margin(bounds, operands, raster):
    """Uniform margin for every baseline per-triangle maximum vertex radius.

    Positive absolute norms bound all source points. The factor two enlarges
    only coarse work; final triangle support and acceptance limits are unchanged.
    Unsupported root projection delegates the whole query to the old selector.
    """
    envelope = clip_envelope(bounds, operands)
    if envelope is None:
        return None
    lo, hi, error = envelope
    w = lo[3] - error[3]
    if (
        w <= 0
        or lo[2] - error[2] <= 0
        or hi[2] + error[2] >= w
        or np.any(lo[:2] - error[:2] <= -w)
        or np.any(hi[:2] + error[:2] >= w)
    ):
        return None
    quotient = upper(np.maximum(abs(lo[:2]), abs(hi[:2])) / lo[3])
    ndc_error = upper(upper(error[:2] + quotient * error[3]) / w)
    return upper(upper(ndc_error * raster / 2) + 8 * EPS32 * raster)


def possible_leaves(bounds, operands, raster, margin, guard, *, batch_boxes=512):
    """All viewport centers, no fine rows, ray hits or reference coverage input."""
    keep = np.ones(len(bounds), bool)
    for begin in range(0, len(bounds), batch_boxes):
        end = min(len(bounds), begin + batch_boxes)
        lo, hi = clip_boxes(bounds[begin:end], operands)
        known = np.isfinite(lo).all(1) & np.isfinite(hi).all(1) & (lo[:, 3] > 0)
        # Root support normally establishes this; uncertainty never drops a leaf.
        rows = np.flatnonzero(known)
        if not len(rows):
            continue
        a, b = lo[rows], hi[rows]
        ratios = np.stack([n[:, :2] / d[:, 3, None] for n in (a, b) for d in (a, b)])
        low = np.nextafter((ratios.min(0) + 1) * raster / 2, -np.inf) - margin - guard
        high = np.nextafter((ratios.max(0) + 1) * raster / 2, np.inf) + margin + guard
        first = np.maximum(0, np.ceil(np.nextafter(low - 0.5, -np.inf)))
        last = np.minimum(raster - 1, np.floor(np.nextafter(high - 0.5, np.inf)))
        keep[begin + rows] = (first <= last).all(1)
    return keep


def group_demand(
    root, witnesses, normal, bounds, view, placements, raster, guard, *, batch_boxes=512
):
    """Metadata only: mask, immediate Whole reason, and projection diagnostics.

    A None mask without a Whole reason requires the original fine query. Callers
    validate layouts/limits first. This does not authorize native raster culling.
    """
    work = {"witness_points_tested": 0, "coarse_margin_px": None}
    if guard >= int(raster.max()):
        return None, "raster_guard_covers_viewport", {**work, "bypass": "guard_whole"}
    if not normal:
        return (
            None,
            "clip_or_arithmetic_unknown",
            {**work, "bypass": "unsupported_source"},
        )
    operands = frame(view, placements)
    if operands is None:
        return (
            None,
            "clip_or_arithmetic_unknown",
            {**work, "bypass": "upload_arithmetic"},
        )
    points = np.repeat(witnesses[:, None, :], 2, axis=1)
    lo, hi = clip_boxes(points, operands)
    work["witness_points_tested"] = len(points)
    outside = (
        (hi[:, 3] <= 0)
        | (hi[:, 2] <= 0)
        | (lo[:, 2] >= hi[:, 3])
        | (lo[:, :2] >= hi[:, 3, None]).any(1)
        | (hi[:, :2] <= -hi[:, 3, None]).any(1)
    )
    if outside.any():
        return (
            None,
            "clip_or_arithmetic_unknown",
            {**work, "bypass": "actual_vertex_outside"},
        )
    margin = support_margin(root, operands, raster.astype(float))
    if margin is None:
        return None, None, {**work, "bypass": "root_projection"}
    keep = possible_leaves(
        bounds, operands, raster, margin, guard, batch_boxes=batch_boxes
    )
    return keep, None, {**work, "coarse_margin_px": margin.tolist(), "bypass": None}
