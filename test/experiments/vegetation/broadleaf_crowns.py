"""Compare extra terminal shoots against leaf stacking at matched physical area."""

from dataclasses import replace

import numpy as np
from biome_models import woody_plan
from outshine_ref.vegetation import AxisLevel, PlantPlan


def architecture(key="oak", seed=718, terminal=False):
    base = woody_plan(key, seed)
    if key not in ("oak", "beech"):
        raise ValueError("oak/beech broadleaf form study required")
    if not terminal:
        return base
    levels = tuple(replace(level, leaf_internode=None) for level in base.levels)
    twig = AxisLevel(
        branches=12,
        length=0.35,
        base_radius=0.0005,
        tip_radius=0.00012,
        tip_scale=0.55,
        length_shape="rounded",
        first=0.12,
        last=0.96,
        attachment_jitter=0.4,
        azimuth_variation=0.7,
        angle=0.95,
        angle_variation=0.35,
        bend=0.5,
        bend_variation=0.2,
        upward_bias=0.12,
        leaf_internode=0.02,
        arrangement="distichous" if key == "beech" else "spiral",
        leaf_sky_alignment=0.3,
    )
    return PlantPlan(seed, (*levels, twig), base.blades)


def scaled_internodes(plan, factor):
    return PlantPlan(
        plan.seed,
        tuple(
            replace(level, leaf_internode=level.leaf_internode * factor)
            if level.leaf_internode is not None
            else level
            for level in plan.levels
        ),
        plan.blades,
    )


def match_area(plan, target_m2):
    """Discrete monotone bisection before any organ/axis placements.

    Leaf-count steps mean the closest sampled area can differ from the target.
    This is a bounded recipe search, not a global coverage optimizer.
    """
    if not np.isfinite(target_m2) or not 1 <= target_m2 <= 250:
        raise ValueError("explicit physical leaf area target within1..250m2 required")
    lo, hi = 0.05, 16.0
    lower, upper = scaled_internodes(plan, lo), scaled_internodes(plan, hi)
    if (
        lower.summary()["leaf_area_m2"] < target_m2
        or upper.summary()["leaf_area_m2"] > target_m2
    ):
        raise ValueError("target is outside this bounded internode recipe range")
    best, error = plan, abs(plan.summary()["leaf_area_m2"] - target_m2)
    for _ in range(40):
        middle = np.sqrt(lo * hi)
        candidate = scaled_internodes(plan, middle)
        area = candidate.summary()["leaf_area_m2"]
        difference = abs(area - target_m2)
        if difference < error:
            best, error = candidate, difference
        if area > target_m2:
            lo = middle
        else:
            hi = middle
    return best
