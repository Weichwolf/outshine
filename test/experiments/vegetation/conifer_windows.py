"""Bind the shared physical window plan to the finite unlit form catalog."""

import json

import numpy as np
from outshine_ref.vegetation.needle_pattern import rotate_y


def pattern_key(pattern):
    return json.dumps(
        {k: v for k, v in pattern.recipe().items() if k not in ("first", "last")},
        sort_keys=True,
    )


def window_count(plant, catalog):
    for order, pattern in enumerate(plant.patterns):
        if pattern is None:
            continue
        info = catalog[pattern_key(pattern)]
        expected = (
            plant.window_summary()["groups_per_window"][order] * pattern.group_size
        )
        if info["needles"] != expected:
            raise ValueError("catalog capacity must match the physical window plan")
    return plant.window_summary()["windows"]


def plan_windows(plant, catalog, max_windows=20000, *, max_axes=20000):
    window_count(plant, catalog)
    plan = plant.windows(max_axes, max_windows)
    if plan is None:
        return None
    return bind_patterns(plant.patterns, plan, catalog)


def bind_patterns(patterns, plan, catalog):
    templates = [catalog[pattern_key(patterns[int(order)])] for order in plan["orders"]]
    if any(
        info["needles"] != capacity
        for info, capacity in zip(templates, plan["capacities"], strict=True)
    ):
        raise ValueError("catalog capacity must match every physical window")
    phase = plan["phases"] - np.array([t["phase"] for t in templates])
    return dict(plan) | {
        "frames": np.einsum("nij,njk->nik", plan["frames"], rotate_y(-phase)),
        "families": np.array([t["family"] for t in templates], np.int32),
        "weights": plan["needle_counts"] / plan["capacities"],
    }
