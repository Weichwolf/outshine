"""Finite source-course cone envelope; stored Float64 coordinates define the model.

The source values are soft controls. This is not observed hydrology. Outward
rounding below assumes IEEE nearest arithmetic and correctly rounded sqrt;
physical facet admission remains a separate check after interpolation.
"""

import hashlib
import json
from dataclasses import dataclass

import numpy as np


def down(value):
    return np.nextafter(value, -np.inf)


def up(value):
    return np.nextafter(value, np.inf)


def sum3(values, rounding):
    return rounding(rounding(values[..., 0] + values[..., 1]) + values[..., 2])


@dataclass(frozen=True)
class CourseBlock:
    key: tuple
    segments: np.ndarray
    levels: np.ndarray
    ids: tuple
    digest: str


@dataclass(frozen=True)
class CourseSummary:
    key: tuple
    count: int
    bounds: tuple | None
    minimum_level_m: float | None
    digest: str
    complete: bool


def course_block(key, segments, levels, ids, *, source_version):
    segments = np.array(segments, dtype=float).reshape(-1, 2, 3)
    levels = np.array(levels, dtype=float).reshape(-1, 2)
    if (
        len(segments) > 8192
        or levels.shape != (len(segments), 2)
        or len(ids) != len(segments)
        or len(set(ids)) != len(ids)
        or any(not isinstance(value, str) or not value for value in ids)
        or not isinstance(source_version, str)
        or not source_version
        or not np.isfinite(segments).all()
        or not np.isfinite(levels).all()
        or np.max(np.abs(segments), initial=0) > 1e8
        or np.max(np.abs(levels), initial=0) > 1e6
    ):
        raise ValueError(
            "bounded finite source segments, levels and stable identities required"
        )
    order = sorted(range(len(ids)), key=lambda i: ids[i])
    segments, levels = segments[order], levels[order]
    ids = tuple(ids[i] for i in order)
    digest = hashlib.sha256(
        json.dumps([key, source_version, ids], separators=(",", ":")).encode()
    )
    for array in (segments, levels):
        digest.update(array.tobytes())
        array.setflags(write=False)
    return CourseBlock(key, segments, levels, ids, digest.hexdigest())


def summarize(block, *, complete=True):
    count = len(block.segments)
    bounds = None
    if count:
        points = block.segments.reshape(-1, 3)
        bounds = (tuple(points.min(axis=0)), tuple(points.max(axis=0)))
    return CourseSummary(
        block.key,
        count,
        bounds,
        float(block.levels.min()) if count else None,
        block.digest,
        complete,
    )


def block_lower_bound(summary, query, slope):
    if not summary.complete:
        return -np.inf
    if not summary.count:
        return np.inf
    low, high = np.asarray(summary.bounds)
    delta = np.maximum(0, np.maximum(down(low - query), down(query - high)))
    square = np.maximum(0, down(delta * delta))
    distance = max(0.0, float(down(np.sqrt(max(0.0, float(sum3(square, down)))))))
    return float(down(summary.minimum_level_m + down(slope * distance)))


def candidate_upper(block, query, slope, parameter):
    """Upper bound at one valid parameter of every original source segment."""
    a, b = block.segments[:, 0], block.segments[:, 1]
    t = np.asarray(parameter)[:, None]
    lower = down(a + down(t * down(b - a)))
    upper = up(a + up(t * up(b - a)))
    delta = np.maximum(np.abs(down(query - upper)), np.abs(up(query - lower)))
    distance = up(np.sqrt(sum3(up(delta * delta), up)))
    height = up(
        block.levels[:, 0] + up(parameter * up(block.levels[:, 1] - block.levels[:, 0]))
    )
    return up(height + up(slope * distance))


def block_minimum(block, query, slope):
    """Analytic line minimizer plus endpoints; no segment is sparsely sampled."""
    a = block.segments[:, 0]
    direction = block.segments[:, 1] - a
    length = np.linalg.norm(direction, axis=1)
    unit = np.divide(
        direction,
        length[:, None],
        out=np.zeros_like(direction),
        where=length[:, None] > 0,
    )
    grade = np.divide(
        block.levels[:, 1] - block.levels[:, 0],
        length,
        out=np.zeros_like(length),
        where=length > 0,
    )
    delta = query - a
    along = (delta * unit).sum(axis=1)
    radius = np.linalg.norm(delta - along[:, None] * unit, axis=1)
    valid = (np.abs(grade) < slope) & (length > 0)
    parameter = np.zeros(len(length))
    position = along[valid] - grade[valid] * radius[valid] / np.sqrt(
        (slope - grade[valid]) * (slope + grade[valid])
    )
    parameter[valid] = np.clip(position / length[valid], 0, 1)
    candidates = np.array([np.zeros(len(length)), np.ones(len(length)), parameter])
    values = np.array([candidate_upper(block, query, slope, t) for t in candidates])
    flat = int(np.argmin(values))
    row, index = np.unravel_index(flat, values.shape)
    best_value = float(values[row, index])
    # Equal numeric values use canonical identity, independent of payload order.
    tied = np.argwhere(values == best_value)
    row, index = min(
        tied, key=lambda p: (block.ids[int(p[1])], float(candidates[tuple(p)]))
    )
    return best_value, block.ids[int(index)], float(candidates[row, index])
