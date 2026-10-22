"""Atomic categorical shadow runs, retaining canonical centres and source density.

Light footprint minus the known-missed view footprint leaves at most two intervals
per grid column. Source knots split each interval into affine-density runs. Actual
box/cap predicates trim their endpoints. Long-run products use gamma differences;
no geometry or source preparation, GEOS or global fine-cell table is required.
"""

import numpy as np
from affine_transmission import sum_log1m_affine
from column_field_query import xy_interval
from urban_parent_conditional import density, expected_shadow, grid, primary_cap


def select_shadow_query(parent, profile, *, force=False):
    """Skip packet dispatch when its global work bound cannot select runs."""
    if not force:
        product = grid(parent)
        pitch, height = product["header"]["pitch_m"], parent.heights[-1]
        if profile["kind"] == "sun":
            light = np.asarray(profile["direction"], float)
            if light[2] <= 0:
                return None
            xy = np.abs(light[:2])
            if height * (xy.sum() - 12 * xy.min()) / (pitch * light[2]) <= 16:
                return None
        elif profile["kind"] == "point":
            light = np.asarray(profile["position"], float)
            low = product["reference_origin_m"][:2]
            high = low + product["shape"] * pitch
            major = np.maximum(np.abs(light[:2] - low), np.abs(light[:2] - high)).max()
            if (
                light[2] > height
                and height * major / ((light[2] - height) * pitch) <= 16
            ):
                return None
    return RowRunShadows(parent, force=force)


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


def subtract(low, high, cut_low, cut_high):
    absent = cut_high < cut_low
    cut_low = np.where(absent, high + 1, cut_low)
    cut_high = np.where(absent, high, cut_high)
    ids = np.tile(np.arange(len(low)), 2)
    lo = np.r_[low, np.maximum(low, cut_high + 1)]
    hi = np.r_[np.minimum(high, cut_low - 1), high]
    valid = lo <= hi
    return ids[valid], lo[valid], hi[valid]


class RowRunShadows:
    def __init__(self, parent, *, run_budget=131072, force=False):
        self.parent = parent
        self.product = grid(parent)
        self.cuts = []
        extent = parent.frame[2] * parent.frame[3]
        pitch = self.product["header"]["pitch_m"]
        for axis, n in enumerate(parent.centre_fraction().shape[::-1]):
            knots = (np.arange(-1, n + 1) + 0.5) * extent / n
            coordinate = (
                knots - self.product["reference_origin_m"][axis]
            ) / pitch - 0.5
            cuts = np.clip(
                np.ceil(coordinate).astype(np.int64), 0, self.product["shape"][axis]
            )
            self.cuts.append(np.unique(np.r_[0, cuts, self.product["shape"][axis]]))
        self.tail = np.r_[np.cumsum(parent.probabilities[::-1])[::-1], 0]
        self.run_budget = run_budget
        self.force = force

    def trim(
        self,
        lo,
        hi,
        valid,
        ids,
        fixed,
        axis,
        points,
        light,
        primary,
        height,
        product,
        work,
        *,
        view,
    ):
        for _ in range(2):
            for lower in [True, False]:
                y = lo if lower else hi
                cell = cells_at(fixed, y, axis)
                if view:
                    cap = primary_cap(product, primary, ids, cell, work)
                    hit = height >= cap
                else:
                    near, far, _ = xy_interval(product, points, light, ids, cell)
                    entry = np.maximum(near, 0)
                    top = (self.parent.heights[-1] - points[ids, 2]) / light[ids, 2]
                    hit = (entry < np.minimum(far, top)) & (
                        points[ids, 2] + entry * light[ids, 2] <= height
                    )
                    work["run_endpoint_tests"] = work.get(
                        "run_endpoint_tests", 0
                    ) + len(ids)
                failed = valid & ~hit
                if lower:
                    lo = lo + failed
                else:
                    hi = hi - failed
        return lo, np.where(valid, hi, lo - 1)

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

    def __call__(self, parent, product, points, light, primary, own, work):
        for key in ["primary_condition_tests", "steps", "cell_ray_visits"]:
            work.setdefault(key, 0)
        work["run_query_receivers"] = work.get("run_query_receivers", 0) + len(points)
        if not len(points):
            return np.empty(0)
        if np.any(light[:, 2] <= 0):
            return expected_shadow(parent, product, points, light, primary, own, work)
        if np.all(points[:, 2] >= parent.heights[-1]):
            work["run_clear_receivers"] = work.get("run_clear_receivers", 0) + len(
                points
            )
            return np.ones(len(points))
        if (
            len(product.get("primary_covered", ()))
            or len(product.get("caster_covered", ()))
            or not np.isfinite(primary[2]).all()
            or np.any(light[:, 2] < 1e-10)
            or parent.frame[6] < 0.15 - 1e-14
        ):
            work["run_fallback_receivers"] = work.get(
                "run_fallback_receivers", 0
            ) + len(points)
            return expected_shadow(parent, product, points, light, primary, own, work)
        pitch = product["header"]["pitch_m"]
        half = product["header"]["footprint_fraction"] / 2
        origin = points - product["reference_origin_m"]
        if not self.force and not self.profitable(origin, light, product):
            work["run_direct_receivers"] = work.get("run_direct_receivers", 0) + len(
                points
            )
            return expected_shadow(parent, product, points, light, primary, own, work)
        p = origin[:, :2] / pitch - 0.5
        axis = (np.abs(light[:, 0]) > np.abs(light[:, 1])).astype(int)
        rows = np.arange(len(points))
        span = np.maximum(parent.heights[-1] - points[:, 2], 0) / light[:, 2]
        endpoint = p + span[:, None] * light[:, :2] / pitch
        ncols = np.ceil(
            np.abs(endpoint[rows, axis] - p[rows, axis]) + 2 * half + 3
        ).astype(int)
        if ncols.sum() > self.run_budget:
            work["run_fallback_receivers"] = work.get(
                "run_fallback_receivers", 0
            ) + len(points)
            return expected_shadow(parent, product, points, light, primary, own, work)
        depth = np.zeros(len(points))
        for level, height in enumerate(parent.heights):
            at = np.flatnonzero(points[:, 2] <= height)
            stop = (height - points[at, 2]) / light[at, 2]
            endpoint = p[at] + stop[:, None] * light[at, :2] / pitch
            a, b = p[at, np.array(axis[at])], endpoint[np.arange(len(at)), axis[at]]
            col0 = np.maximum(np.floor(np.minimum(a, b) - half).astype(int), 0)
            col1 = np.minimum(
                np.ceil(np.maximum(a, b) + half).astype(int),
                product["shape"][axis[at]] - 1,
            )
            repeated, fixed = repeat_intervals(col0, col1)
            ids = at[repeated]
            ax = axis[ids]
            lo, hi, valid = column_bounds(p[ids], endpoint[repeated], fixed, ax, half)
            lo = np.maximum(lo, 0)
            hi = np.minimum(hi, product["shape"][1 - ax] - 1)
            lo, hi = self.trim(
                lo,
                hi,
                valid,
                ids,
                fixed,
                ax,
                origin,
                light,
                primary,
                height,
                product,
                work,
                view=False,
            )
            valid = lo <= hi
            ids, fixed, ax, lo, hi = (v[valid] for v in [ids, fixed, ax, lo, hi])
            self.view_runs(
                depth,
                ids,
                fixed,
                ax,
                lo,
                hi,
                p,
                origin,
                light,
                primary,
                own,
                height,
                level,
                product,
                work,
            )
        return np.exp(-np.maximum(depth, 0))

    def profitable(self, origin, light, product):
        pitch = product["header"]["pitch_m"]
        span = np.maximum(self.parent.heights[-1] - origin[:, 2], 0) / light[:, 2]
        upper = span[:, None] * np.abs(light[:, :2]) / pitch
        if float(upper.sum()) <= 12 * float(upper.min(axis=1).sum()) + 16 * len(origin):
            return False
        parallel = light[:, :2] == 0
        bounds = np.array([np.zeros(2), product["shape"] * pitch])
        t = np.divide(
            bounds[None] - origin[:, None, :2],
            light[:, None, :2],
            out=np.zeros((len(origin), 2, 2)),
            where=~parallel[:, None],
        )
        entry = np.maximum(0, np.where(parallel, -np.inf, t.min(axis=1)).max(axis=1))
        exit = np.minimum(
            (self.parent.heights[-1] - origin[:, 2]) / light[:, 2],
            np.where(parallel, np.inf, t.max(axis=1)).min(axis=1),
        )
        outside = np.any(
            parallel & ((origin[:, :2] < bounds[0]) | (origin[:, :2] > bounds[1])),
            axis=1,
        )
        distance = np.where(outside, 0, np.maximum(exit - entry, 0))
        component = distance[:, None] * np.abs(light[:, :2]) / pitch
        return float(component.sum()) > 12 * float(
            component.min(axis=1).sum()
        ) + 16 * len(origin)

    def view_runs(
        self,
        depth,
        ids,
        fixed,
        axis,
        lo,
        hi,
        p,
        origin,
        light,
        primary,
        own,
        height,
        level,
        product,
        work,
    ):
        vo, vd, limit = primary
        times = np.divide(
            np.c_[-vo[:, 2], height - vo[:, 2]],
            vd[:, 2, None],
            out=np.zeros((len(vo), 2)),
            where=vd[:, 2, None] != 0,
        )
        begin = np.maximum(0.1, np.where(vd[:, 2] == 0, -np.inf, times.min(axis=1)))
        end = np.minimum(limit, np.where(vd[:, 2] == 0, np.inf, times.max(axis=1)))
        active = (end > begin) & (
            (vd[:, 2] != 0) | ((vo[:, 2] >= 0) & (vo[:, 2] <= height))
        )
        begin = np.where(active, begin, 0)
        end = np.where(active, end, 0)
        pitch = product["header"]["pitch_m"]
        first = (vo[:, :2] + begin[:, None] * vd[:, :2]) / pitch - 0.5
        last = (vo[:, :2] + end[:, None] * vd[:, :2]) / pitch - 0.5
        a, b, valid = column_bounds(
            first[ids],
            last[ids],
            fixed,
            axis,
            product["header"]["footprint_fraction"] / 2,
        )
        a, b = self.trim(
            a,
            b,
            valid & active[ids],
            ids,
            fixed,
            axis,
            origin,
            light,
            primary,
            height,
            product,
            work,
            view=True,
        )
        pieces, lo, hi = subtract(lo, hi, a, b)
        ids, fixed, axis = (v[pieces] for v in [ids, fixed, axis])
        same = fixed == own[ids, axis]
        value = own[ids, 1 - axis]
        cutlo = np.where(same, value, hi + 1)
        cuthi = np.where(same, value, hi)
        pieces, lo, hi = subtract(lo, hi, cutlo, cuthi)
        ids, fixed, axis = (v[pieces] for v in [ids, fixed, axis])
        values = self.sum_runs(ids, fixed, axis, lo, hi, level, product, work)
        np.add.at(depth, ids, values)
        work["run_columns"] = work.get("run_columns", 0) + len(ids)
