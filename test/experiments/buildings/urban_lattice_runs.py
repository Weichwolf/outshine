"""Canonical lattice intervals and affine source-density log products.

Shared by primary-prefix and view-conditioned shadow consumers; no cell table.
"""

import numpy as np
from affine_transmission import sum_log1m_affine
from urban_parent_conditional import density


def repeat_intervals(low, high):
    sizes = np.maximum(high - low + 1, 0)
    ids = np.repeat(np.arange(len(low)), sizes)
    starts = np.cumsum(sizes) - sizes
    values = low[ids] + np.arange(len(ids)) - starts[ids]
    return ids, values


def column_bounds(first, last, fixed, axis, half):
    """Conservative integer candidates; actual box predicates trim endpoints."""
    row = np.arange(len(first))
    a, b = first[row, axis], last[row, axis]
    c, e = first[row, 1 - axis], last[row, 1 - axis]
    dx, dy = b - a, e - c
    support = half * (np.abs(dx) + np.abs(dy))
    left = (
        np.divide(dy * (fixed - a) - support, dx, out=np.zeros(len(dx)), where=dx != 0)
        + c
    )
    right = (
        np.divide(dy * (fixed - a) + support, dx, out=np.zeros(len(dx)), where=dx != 0)
        + c
    )
    low, high = np.minimum(c, e) - half, np.maximum(c, e) + half
    low = np.where(dx != 0, np.maximum(low, np.minimum(left, right)), low)
    high = np.where(dx != 0, np.minimum(high, np.maximum(left, right)), high)
    guard = 32 * np.finfo(float).eps * (1 + np.maximum(np.abs(low), np.abs(high)))
    valid = (
        (fixed >= np.minimum(a, b) - half - guard)
        & (fixed <= np.maximum(a, b) + half + guard)
        & (low <= high + guard)
    )
    return (
        np.floor(low - guard).astype(np.int64),
        np.ceil(high + guard).astype(np.int64),
        valid,
    )


def cells_at(fixed, varying, axis):
    result = np.column_stack((fixed, varying))
    result[axis == 1] = result[axis == 1, ::-1]
    return result


class AffineDensityRuns:
    def __init__(self, parent, product):
        self.parent = parent
        self.cuts = []
        extent = parent.frame[2] * parent.frame[3]
        pitch = product["header"]["pitch_m"]
        for axis, n in enumerate(parent.centre_fraction().shape[::-1]):
            knots = (np.arange(-1, n + 1) + 0.5) * extent / n
            coordinate = (knots - product["reference_origin_m"][axis]) / pitch - 0.5
            cuts = np.clip(
                np.ceil(coordinate).astype(np.int64), 0, product["shape"][axis]
            )
            self.cuts.append(np.unique(np.r_[0, cuts, product["shape"][axis]]))
        self.tail = np.r_[np.cumsum(parent.probabilities[::-1])[::-1], 0]

    def sum_runs(self, ids, fixed, axis, lo, hi, level, product, work):
        values = np.zeros(len(ids))
        for orientation in [0, 1]:
            at = np.flatnonzero(axis == orientation)
            cuts = self.cuts[1 - orientation]
            first = np.searchsorted(cuts, lo[at], side="right") - 1
            last = np.searchsorted(cuts, hi[at], side="right") - 1
            run, segment = repeat_intervals(first, last)
            source = at[run]
            a = np.maximum(lo[source], cuts[segment])
            b = np.minimum(hi[source], cuts[segment + 1] - 1)
            count = b - a + 1
            start = cells_at(fixed[source], a, axis[source])
            end = cells_at(fixed[source], b, axis[source])
            q0 = density(self.parent, product, start)
            q1 = density(self.parent, product, end)
            depth = sum_log1m_affine(
                q0, q1, count, self.tail[level + 1]
            ) - sum_log1m_affine(q0, q1, count, self.tail[level])
            np.add.at(values, source, depth)
            work["run_named_arrays_peak_bytes"] = max(
                work.get("run_named_arrays_peak_bytes", 0),
                sum(
                    v.nbytes
                    for v in [
                        values,
                        source,
                        run,
                        segment,
                        a,
                        b,
                        count,
                        start,
                        end,
                        q0,
                        q1,
                        depth,
                    ]
                ),
            )
            work["run_source_endpoint_samples"] = work.get(
                "run_source_endpoint_samples", 0
            ) + 2 * len(source)
            work["affine_runs"] = work.get("affine_runs", 0) + len(source)
            work["affine_cells"] = work.get("affine_cells", 0) + int(count.sum())
            work["gamma_cells"] = work.get("gamma_cells", 0) + int(
                count[count > 16].sum()
            )
        return values
