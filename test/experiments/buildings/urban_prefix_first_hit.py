"""Invert categorical prefix transmission at whole-cell slab boundaries.

The final slab uses the original first-hit events. Ambiguous floating comparisons
and unsupported/short packets retain full DDA; no certified culling bound follows.
"""

import numpy as np
from urban_first_hit import merge_work, sample_first_hits, sample_inputs, sample_packets
from urban_parent_conditional import validate_eye
from urban_prefix_runs import clipped_span


def slab_limits(model, origins, directions, stops):
    product = model.product
    local = origins - product["reference_origin_m"]
    begin, end, active = clipped_span(product, local, directions, stops)
    axis = np.abs(directions[:, :2]).argmax(axis=1)
    rows = np.arange(len(origins))
    speed = directions[rows, axis]
    sign = np.where(speed < 0, -1, 1)
    pitch = product["header"]["pitch_m"]
    offset = local[rows, axis]
    first = (offset + begin * speed) / pitch * sign
    last = (offset + end * speed) / pitch * sign
    low = np.floor(np.nextafter(first, np.inf)).astype(np.int64)
    high = np.floor(np.nextafter(last, -np.inf)).astype(np.int64)
    return begin, end, active, low, high, speed, sign, offset


class PrefixFirstHits:
    def __init__(self, model, *, force=False):
        self.model = model
        self.force = force

    def narrow(
        self,
        o,
        d,
        low,
        high,
        speed,
        sign,
        offset,
        target,
        bounds,
        before,
        selected,
        ambiguous,
        work,
    ):
        model = self.model
        while np.any(selected & (low < high)):
            ids = np.flatnonzero(selected & (low < high))
            middle = (low[ids] + high[ids] + 1) // 2
            stop = (
                middle * sign[ids] * model.product["header"]["pitch_m"] - offset[ids]
            ) / speed[ids]
            value, counts = model.transmission(o[ids], d[ids], stop)
            merge_work(work, counts, "search_")
            work["known_input_copy_bytes"] += 56 * len(ids)
            close = np.abs(value - target[ids]) <= 1e-10
            ambiguous[ids[close]] = True
            selected[ids[close]] = False
            left = value < target[ids]
            high[ids[left]] = middle[left] - 1
            low[ids[~left]] = middle[~left]
            before[ids[~left]] = value[~left]
            bounds[ids[~left], 0] = stop[~left]
            work["search_rounds"] += 1
            work["search_comparisons"] += len(ids)

    def sample(self, origins, directions, uniform, stops=None):
        o, d, u, stops = sample_inputs(origins, directions, uniform, stops)
        stops = np.full(len(o), np.inf) if stops is None else np.asarray(stops, float)
        model = self.model
        validate_eye(model.parent, model.product, o, d)
        begin, end, active, low, high, speed, sign, offset = slab_limits(
            model, o, d, stops
        )
        span = np.maximum(high - low + 1, 0)
        minor = np.min(np.abs(d[:, :2]), axis=1)
        thin = minor * (end - begin) / model.product["header"]["pitch_m"] <= 4
        eligible = active & (span > 256) & thin
        if self.force:
            eligible = active & (speed != 0)
        if model.parent.frame[5] >= 1 or len(model.product.get("primary_covered", ())):
            eligible[:] = False
        result = {
            "depth": np.full(len(o), np.inf),
            "normal": np.zeros_like(o),
            "cell": np.full((len(o), 2), -1, np.int64),
            "height_index": np.full(len(o), -1, np.int64),
        }
        work = {
            "search_rounds": 0,
            "search_comparisons": 0,
            "known_input_copy_bytes": 0,
        }
        bounds, before = np.c_[begin, end], np.ones(len(o))
        pending = eligible.copy()
        ids = np.flatnonzero(eligible)
        if len(ids) and not self.force:
            answer, counts, state = sample_packets(
                model.parent,
                model.product,
                o[ids],
                d[ids],
                u[ids],
                stops[ids],
                step_limit=64,
            )
            for key, value in result.items():
                value[ids] = answer[key]
            merge_work(work, counts, "scout_")
            work["known_input_copy_bytes"] += 64 * len(ids)
            pending[ids] = ~np.isfinite(answer["depth"]) & (state[0] < end[ids])
            bounds[ids, 0], before[ids] = state
            coordinate = (
                (offset[ids] + state[0] * speed[ids])
                / model.product["header"]["pitch_m"]
                * sign[ids]
            )
            low[ids] = np.floor(np.nextafter(coordinate, np.inf)).astype(np.int64)
        total = np.ones(len(o))
        ids = np.flatnonzero(pending)
        if len(ids):
            total[ids], counts = model.transmission(o[ids], d[ids], stops[ids])
            merge_work(work, counts, "total_")
            work["known_input_copy_bytes"] += 56 * len(ids)
        target = 1 - u
        guard = 1e-10
        ambiguous = pending & (np.abs(total - target) <= guard)
        selected = pending & ~ambiguous & (total < target)
        self.narrow(
            o,
            d,
            low,
            high,
            speed,
            sign,
            offset,
            target,
            bounds,
            before,
            selected,
            ambiguous,
            work,
        )
        ids = np.flatnonzero(selected)
        if len(ids):
            stop = (
                (low[ids] + 1) * sign[ids] * model.product["header"]["pitch_m"]
                - offset[ids]
            ) / speed[ids]
            bounds[ids, 1] = np.minimum(stop, end[ids])
            answer, counts = sample_first_hits(
                model.parent,
                model.product,
                o[ids],
                d[ids],
                u[ids],
                stops[ids],
                resume=(bounds[ids], before[ids]),
            )
            failed = ~np.isfinite(answer["depth"])
            ambiguous[ids[failed]] = True
            for key, value in result.items():
                value[ids] = answer[key]
            merge_work(work, counts, "tail_")
            work["known_input_copy_bytes"] += 88 * len(ids)
        fallback = ~eligible | ambiguous
        ids = np.flatnonzero(fallback)
        if len(ids):
            answer, counts = sample_first_hits(
                model.parent, model.product, o[ids], d[ids], u[ids], stops[ids]
            )
            for key, value in result.items():
                value[ids] = answer[key]
            merge_work(work, counts, "fallback_")
            work["known_input_copy_bytes"] += 64 * len(ids)
        work["inverted_receivers"] = int((pending & ~ambiguous).sum())
        work["scout_resolved_receivers"] = int((eligible & ~pending).sum())
        work["fallback_receivers"] = len(ids)
        work["ambiguous_receivers"] = int(ambiguous.sum())
        work["sampled_hits"] = int(np.isfinite(result["depth"]).sum())
        return result, work
