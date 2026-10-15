"""Geometry-free candidate lookup from the experiment's compiled numeric tables."""

import numpy as np

MIXED = 1 << 15


def points(latitude, longitude, *, max_queries=1000000):
    latitude, longitude = np.broadcast_arrays(latitude, longitude)
    if (
        latitude.size > max_queries
        or not np.isfinite(latitude).all()
        or not np.isfinite(longitude).all()
        or np.any(abs(latitude) > 90)
        or np.any(abs(longitude) > 180)
    ):
        raise ValueError("bounded finite WGS84 query coordinates required")
    return np.column_stack(
        (np.where(longitude.ravel() == 180, -180, longitude.ravel()), latitude.ravel())
    )


def cell_rows(level, xy):
    if type(level) is not int or not 0 <= level <= 10:
        raise ValueError("bounded GeoCellId level required")
    side = 1 << level
    x = np.searchsorted(np.linspace(-180, 180, side + 1), xy[:, 0], side="right") - 1
    y = np.searchsorted(np.linspace(-90, 90, side + 1), xy[:, 1], side="right") - 1
    return np.minimum(y, side - 1) * side + x


def candidate_pairs(
    level, table, offsets, candidates, latitude, longitude, *, max_pairs=4000000
):
    """Use trusted experiment-compiler arrays; no polygons or botanical decisions.

    This numeric layout is experimental, not a public/untrusted asset decoder.
    """
    if type(max_pairs) is not int or not 0 <= max_pairs <= 16000000:
        raise ValueError("bounded candidate output required")
    values = table[cell_rows(level, points(latitude, longitude))]
    mixed = values >= MIXED
    counts = (values != 0).astype(np.int64)
    buckets = values[mixed].astype(np.int64) - MIXED
    counts[mixed] = offsets[buckets + 1] - offsets[buckets]
    total = int(counts.sum())
    if total > max_pairs:
        raise ValueError("candidate output budget exceeded before pair allocation")
    rows = np.repeat(np.arange(len(values)), counts)
    within = np.arange(total) - np.repeat(np.cumsum(counts) - counts, counts)
    codes = values[rows]
    is_mixed = codes >= MIXED
    regions = codes.astype(np.int64) - 1
    bucket = codes[is_mixed].astype(np.int64) - MIXED
    regions[is_mixed] = candidates[offsets[bucket] + within[is_mixed]]
    return np.array((rows, regions))
