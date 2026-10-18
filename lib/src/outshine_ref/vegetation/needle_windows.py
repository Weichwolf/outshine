"""Statistical physical windows with complete source ranges and deferred needles."""

from types import MappingProxyType

import numpy as np

from .curve_frame import arc_frame
from .plan_arrays import detail_budget, immutable_array, repeated_prefix
from .plant_levels import GOLDEN_ANGLE


def capacities(plant, nominal_span_m, max_groups):
    span = np.asarray(nominal_span_m)
    if (
        span.shape != ()
        or span.dtype.kind not in "fiu"
        or not np.isfinite(span)
        or not 0 < span <= 1000
    ):
        raise ValueError(
            "finite positive nominal window span up to 1000 metres required"
        )
    if type(max_groups) is not int or not 1 <= max_groups <= 4096:
        raise ValueError("one to 4096 needle groups per window required")
    return tuple(
        0 if p is None else max(1, min(max_groups, int(float(span) / p.spacing)))
        for p in plant.patterns
    )


def window_summary(plant, nominal_span_m=0.08, max_groups=64):
    groups = capacities(plant, nominal_span_m, max_groups)
    total = 0
    for axes, level, slots, pattern, capacity in zip(
        plant.wood.axis_counts,
        plant.wood.levels,
        plant.slot_counts,
        plant.patterns,
        groups,
    ):
        if pattern is not None:
            needles = capacity * pattern.group_size
            total += (
                axes
                // level.branches
                * sum((n + needles - 1) // needles for n in slots)
            )
    return {
        "identity": plant.identity,
        "axes": sum(plant.wood.axis_counts),
        "windows": total,
        "needles": sum(plant.needle_counts),
        "groups_per_window": groups,
    }


def _axis_window_values(axis, pattern, needles, capacity, seed, first):
    starts = np.arange(0, needles, capacity, dtype=np.int64)
    counts = np.minimum(capacity, needles - starts)
    nodes, node_counts = starts / pattern.group_size, counts / pattern.group_size
    t = pattern.first + (nodes + (node_counts - 1) / 2) * pattern.spacing / axis.length
    centre, tangent, lateral, normal = arc_frame(axis.length, axis.bend, 0, t)
    frames = np.stack((lateral, tangent, normal), axis=-1)
    phase = np.random.default_rng(np.random.SeedSequence((seed, axis.id))).uniform(
        -np.pi, np.pi
    )
    return {
        "origins": centre @ axis.frame.T + axis.origin,
        "frames": np.einsum("ij,njk->nik", axis.frame, frames),
        "phases": phase + nodes * GOLDEN_ANGLE,
        "axis_ids": np.full(len(starts), axis.id, np.uint64),
        "orders": np.full(len(starts), len(axis.path), np.uint8),
        "parent_parameters": t,
        "first_needle_ids": starts.astype(np.uint64) + np.uint64(first),
        "needle_counts": counts.astype(np.uint32),
        "capacities": np.full(len(starts), capacity, np.uint32),
    }


def window_tables(plant, max_axes, max_windows, nominal_span_m=0.08, max_groups=64):
    detail_budget(max_axes)
    detail_budget(max_windows)
    summary = window_summary(plant, nominal_span_m, max_groups)
    if summary["axes"] > max_axes or summary["windows"] > max_windows:
        return None
    columns = {
        key: []
        for key in (
            "origins",
            "frames",
            "phases",
            "axis_ids",
            "orders",
            "parent_parameters",
            "first_needle_ids",
            "needle_counts",
            "capacities",
        )
    }
    for path in plant.wood.paths():
        depth, ordinal = len(path), plant.wood.ordinal(path)
        slot = path[-1] if path else 0
        needles = plant.slot_counts[depth][slot]
        if not needles:
            continue
        pattern, axis = plant.patterns[depth], plant.wood.axis(path)
        capacity = summary["groups_per_window"][depth] * pattern.group_size
        first = sum(plant.needle_counts[:depth]) + repeated_prefix(
            plant.slot_counts[depth], ordinal
        )
        values = _axis_window_values(
            axis, pattern, needles, capacity, plant.wood.seed, first
        )
        for key, value in values.items():
            columns[key].append(value)
    result = {
        key: immutable_array(np.concatenate(parts)) for key, parts in columns.items()
    }
    return MappingProxyType(result)
