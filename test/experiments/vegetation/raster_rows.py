"""Bounded scanline candidates; final barycentric arithmetic matches raster_batch."""

import numpy as np


def groups(counts, budget):
    cumulative = np.r_[0, np.cumsum(counts)]
    start = 0
    while start < len(counts):
        stop = min(
            len(counts),
            int(
                np.searchsorted(cumulative, cumulative[start] + budget, side="right")
                - 1
            ),
        )
        stop = max(start + 1, stop)
        yield start, stop
        start = stop


def row_intervals(triangles, first, last, ab, ac, determinant, triangle, y, size, work):
    a, b, det = ab[triangle], ac[triangle], determinant[triangle]
    origin = triangles[triangle, 0]
    py = y + 0.5 - origin[:, 1]
    derivative = np.c_[(a[:, 1] - b[:, 1]) / det, b[:, 1] / det, -a[:, 1] / det]
    wb, wc = -py * b[:, 0] / det, a[:, 0] * py / det
    intercept = np.c_[1 - wb - wc, wb, wc]
    # Allow roundoff before pruning, then apply the original threshold unchanged.
    magnitude = (size + abs(origin[:, 0])) * (abs(a[:, 1]) + abs(b[:, 1]))
    magnitude += (size + abs(origin[:, 1])) * (abs(a[:, 0]) + abs(b[:, 0]))
    allowance = 64 * np.finfo(float).eps * (1 + magnitude / abs(det))
    roots = np.divide(
        -1e-10 - allowance[:, None] - intercept,
        derivative,
        out=np.zeros_like(intercept),
        where=derivative != 0,
    )
    lower = np.where(derivative > 0, roots, -np.inf).max(1)
    upper = np.where(derivative < 0, roots, np.inf).min(1)
    parallel_outside = np.any(
        (derivative == 0) & (intercept < -1e-10 - allowance[:, None]), axis=1
    )
    lo, hi = first[triangle, 0], last[triangle, 0]
    lower = np.maximum(lower, lo + 0.5 - origin[:, 0])
    upper = np.minimum(upper, hi - 0.5 - origin[:, 0])
    valid = (lower <= upper) & ~parallel_outside
    x0 = np.ceil(np.where(valid, lower + origin[:, 0] - 0.5, 0) - 1e-8).astype(np.int64)
    x1 = (
        np.floor(np.where(valid, upper + origin[:, 0] - 0.5, -1) + 1e-8).astype(
            np.int64
        )
        + 1
    )
    x0, x1 = np.maximum(x0, lo), np.minimum(x1, hi)
    uncertain = allowance > 1e-5
    x0[uncertain], x1[uncertain] = lo[uncertain], hi[uncertain]
    work["unpruned_rows"] += int(uncertain.sum())
    count = np.maximum(x1 - x0, 0)
    count[~valid & ~uncertain] = 0
    return x0, count


def fragments(vertices, faces, lower, upper, size, pair_budget=262144, work=None):
    if (
        type(size) is not int
        or not 1 <= size <= 1024
        or type(pair_budget) is not int
        or not size * size <= pair_budget <= 4_194_304
    ):
        raise ValueError(
            "bounded image/pair work with room for one full triangle required"
        )
    work = {} if work is None else work
    work.update(
        candidate_rows=0,
        candidate_pairs=0,
        max_pair_batch=0,
        max_row_batch=0,
        unpruned_rows=0,
    )
    lower, upper = np.asarray(lower), np.asarray(upper)
    triangles = ((vertices[:, :2] - lower) / (upper - lower) * size)[faces]
    first = np.maximum(np.floor(triangles.min(1)).astype(np.int64), 0)
    last = np.minimum(np.ceil(triangles.max(1)).astype(np.int64), size)
    span = np.maximum(last - first, 0)
    ab, ac = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    determinant = ab[:, 0] * ac[:, 1] - ab[:, 1] * ac[:, 0]
    rows = np.where((abs(determinant) >= 1e-12) & (span[:, 0] > 0), span[:, 1], 0)
    for start, stop in groups(rows, min(32768, pair_budget)):
        count = rows[start:stop]
        total = int(count.sum())
        if not total:
            continue
        triangle = np.repeat(np.arange(start, stop), count)
        y = (
            first[triangle, 1]
            + np.arange(total)
            - np.repeat(np.cumsum(count) - count, count)
        )
        x0, counts = row_intervals(
            triangles, first, last, ab, ac, determinant, triangle, y, size, work
        )
        work["candidate_rows"] += total
        work["max_row_batch"] = max(work["max_row_batch"], total)
        for begin, end in groups(counts, pair_budget):
            counts_batch = counts[begin:end]
            total_pairs = int(counts_batch.sum())
            work["candidate_pairs"] += total_pairs
            work["max_pair_batch"] = max(work["max_pair_batch"], total_pairs)
            if not total_pairs:
                continue
            row = np.repeat(np.arange(begin, end), counts_batch)
            index = np.arange(total_pairs) - np.repeat(
                np.cumsum(counts_batch) - counts_batch, counts_batch
            )
            t, x, yy = triangle[row], x0[row] + index, y[row]
            px, py = x + 0.5 - triangles[t, 0, 0], yy + 0.5 - triangles[t, 0, 1]
            b = (px * ac[t, 1] - py * ac[t, 0]) / determinant[t]
            c = (ab[t, 0] * py - ab[t, 1] * px) / determinant[t]
            weights = np.c_[1 - b - c, b, c]
            valid = np.all(weights >= -1e-10, axis=1)
            t, weights, x, yy = t[valid], weights[valid], x[valid], yy[valid]
            z = np.einsum("ni,ni->n", weights, vertices[faces[t], 2])
            yield t, weights, yy * size + x, z
