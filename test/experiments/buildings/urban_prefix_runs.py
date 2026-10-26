"""Primary categorical prefix transmission from canonical affine-density runs.

This provides probability, not first-hit geometry, radiance or a World certificate.
Short prefixes and exhausted column budgets retain the current DDA control.
"""

import numpy as np
from column_field_query import clip_height, xy_interval
from urban_lattice_runs import (
    AffineDensityRuns,
    cells_at,
    column_bounds,
    repeat_intervals,
)
from urban_parent_conditional import covered_grid, grid
from urban_parent_prefix import dda_prefix


def clipped_span(product, origin, direction, stops):
    pitch = product["header"]["pitch_m"]
    bounds = np.array([np.zeros(2), product["shape"] * pitch])
    parallel = direction[:, :2] == 0
    t = np.divide(
        bounds[None] - origin[:, None, :2],
        direction[:, None, :2],
        out=np.zeros((len(origin), 2, 2)),
        where=~parallel[:, None],
    )
    first = np.maximum(0.1, np.where(parallel, -np.inf, t.min(axis=1)).max(axis=1))
    last = np.minimum(stops, np.where(parallel, np.inf, t.max(axis=1)).min(axis=1))
    first, last = clip_height(product, origin, direction, first, last)
    outside = np.any(
        parallel & ((origin[:, :2] < bounds[0]) | (origin[:, :2] >= bounds[1])), axis=1
    )
    phase = origin[:, :2] - np.floor(origin[:, :2] / pitch) * pitch
    guard = 8 * np.finfo(float).eps * (np.abs(origin[:, :2]) + pitch)
    edge = (1 - product["header"]["footprint_fraction"]) * pitch / 2
    outside |= np.any(
        parallel & ((phase < edge - guard) | (phase > pitch - edge + guard)), axis=1
    )
    active = ~outside & (first < last)
    return np.where(active, first, 0), np.where(active, last, 0), active


class PrefixRuns:
    def __init__(self, parent, *, column_budget=131072, force=False):
        self.parent = parent
        self.product = grid(parent)
        self.density = AffineDensityRuns(parent, self.product)
        self.column_budget = column_budget
        self.force = force

    def trim(
        self,
        lo,
        hi,
        valid,
        ids,
        fixed,
        axis,
        origin,
        direction,
        begin,
        end,
        height,
        work,
    ):
        product = self.product
        dz, z = direction[ids, 2], origin[ids, 2]
        t = np.divide(
            np.c_[-z, height - z],
            dz[:, None],
            out=np.zeros((len(ids), 2)),
            where=dz[:, None] != 0,
        )
        near_z = np.where(dz == 0, -np.inf, t.min(axis=1))
        far_z = np.where(dz == 0, np.inf, t.max(axis=1))
        vertical = (dz != 0) | ((z >= 0) & (z <= height))
        for _ in range(2):
            for lower in [True, False]:
                varying = lo if lower else hi
                cell = cells_at(fixed, varying, axis)
                near, far, _ = xy_interval(product, origin, direction, ids, cell)
                entry = np.maximum(np.maximum(near, near_z), begin[ids])
                hit = vertical & (entry < end[ids]) & (entry <= np.minimum(far, far_z))
                failed = valid & ~hit
                if lower:
                    lo = lo + failed
                else:
                    hi = hi - failed
                valid &= lo <= hi
                work["endpoint_tests"] += len(ids)
        return lo, np.where(valid, hi, lo - 1)

    def accumulate(self, origin, direction, begin, end, axis, active, work):
        product = self.product
        pitch = product["header"]["pitch_m"]
        half = product["header"]["footprint_fraction"] / 2
        depth = np.zeros(len(origin))
        for level, height in enumerate(self.parent.heights):
            first, last = clip_height(
                {"height_bounds": [0, height]}, origin, direction, begin, end
            )
            at = np.flatnonzero(active & (first <= last))
            p = (origin[at, :2] + first[at, None] * direction[at, :2]) / pitch - 0.5
            q = (origin[at, :2] + last[at, None] * direction[at, :2]) / pitch - 0.5
            rows = np.arange(len(at))
            a, b = p[rows, axis[at]], q[rows, axis[at]]
            col0 = np.maximum(np.floor(np.minimum(a, b) - half).astype(np.int64), 0)
            col1 = np.minimum(
                np.ceil(np.maximum(a, b) + half).astype(np.int64),
                product["shape"][axis[at]] - 1,
            )
            local, fixed = repeat_intervals(col0, col1)
            ids = at[local]
            ax = axis[ids]
            lo, hi, valid = column_bounds(p[local], q[local], fixed, ax, half)
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
                direction,
                begin,
                end,
                height,
                work,
            )
            valid = lo <= hi
            ids, fixed, ax, lo, hi = (v[valid] for v in [ids, fixed, ax, lo, hi])
            values = self.density.sum_runs(ids, fixed, ax, lo, hi, level, product, work)
            np.add.at(depth, ids, values)
            work["run_columns"] += len(ids)
            work["prefix_named_arrays_peak_bytes"] = max(
                work.get("prefix_named_arrays_peak_bytes", 0),
                sum(
                    v.nbytes
                    for v in [
                        depth,
                        first,
                        last,
                        at,
                        p,
                        q,
                        local,
                        fixed,
                        ids,
                        ax,
                        lo,
                        hi,
                        values,
                    ]
                ),
            )
        return np.exp(-np.maximum(depth, 0))

    def transmission(self, origins, directions, stops=None):
        o, d = np.asarray(origins, float), np.asarray(directions, float)
        if o.ndim != 2 or o.shape[1] != 3 or d.shape != o.shape:
            raise ValueError("finite above-base prefix geometry required")
        stops = np.full(len(o), np.inf) if stops is None else np.asarray(stops, float)
        if (
            o.ndim != 2
            or o.shape[1] != 3
            or d.shape != o.shape
            or stops.shape != (len(o),)
            or not np.isfinite(o).all()
            or not np.isfinite(d).all()
            or np.any(np.linalg.norm(d, axis=1) == 0)
            or np.any(o[:, 2] < 0)
            or np.any(np.isnan(stops) | (stops < 0.1))
        ):
            raise ValueError("finite above-base prefix geometry required")
        regions = self.product.get("primary_covered", ())
        if covered_grid(self.product, regions):
            return np.ones(len(o)), {
                "prefix_queries": len(o),
                "ready_bypass_rays": len(o),
            }
        if len(regions):
            result, work = dda_prefix(self.parent, o, d, stops, product=self.product)
            return result, {"prefix_queries": len(o), "masked_dda": work}
        local = o - self.product["reference_origin_m"]
        begin, end, active = clipped_span(self.product, local, d, stops)
        span = (
            (end - begin)[:, None]
            * np.abs(d[:, :2])
            / self.product["header"]["pitch_m"]
        )
        axis = (np.abs(d[:, 0]) > np.abs(d[:, 1])).astype(np.int64)
        rows = np.arange(len(o))
        columns = np.ceil(span[rows, axis] + self.parent.frame[5] + 3).astype(np.int64)
        eligible = active.copy() if self.force else active & (span.sum(axis=1) > 16)
        eligible &= columns <= self.column_budget
        eligible &= np.cumsum(np.where(eligible, columns, 0)) <= self.column_budget
        if self.parent.frame[6] < 0.15 - 1e-14:
            eligible[:] = False
        work = {
            "prefix_queries": len(o),
            "run_receivers": int(eligible.sum()),
            "dda_receivers": int((active & ~eligible).sum()),
            "clear_receivers": int((~active).sum()),
            "run_columns": 0,
            "endpoint_tests": 0,
            "known_selected_input_copy_bytes": 72 * int(eligible.sum())
            + 56 * int((active & ~eligible).sum()),
        }
        result = np.ones(len(o))
        ids = np.flatnonzero(eligible)
        if len(ids):
            result[ids] = self.accumulate(
                local[ids],
                d[ids],
                begin[ids],
                end[ids],
                axis[ids],
                np.ones(len(ids), bool),
                work,
            )
        ids = np.flatnonzero(active & ~eligible)
        if len(ids):
            result[ids], counts = dda_prefix(self.parent, o[ids], d[ids], stops[ids])
            work.update(dda=counts)
        return result, work
