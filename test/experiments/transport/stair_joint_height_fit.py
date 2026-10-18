"""Fit free flat junction levels, then pin their exact Float32 heights."""

import numpy as np
import shapely


def level_groups(xy, plans):
    if not 1 <= len(plans) <= 20:
        raise ValueError("one to twenty explicit joint level regions required")
    used, groups = set(), []
    points = shapely.points(xy)
    for plan in plans:
        group = np.flatnonzero(shapely.dwithin(plan["shape"], points, 1e-8))
        if len(group) < 3 or used.intersection(group):
            raise ValueError("disjoint nonempty whole joint level groups required")
        used.update(group)
        groups.append(group)
    return groups


def bound_levels(plans, heights):
    heights = np.asarray(heights)
    if (
        heights.shape != (len(plans),)
        or not np.isfinite(heights).all()
        or not np.array_equal(heights, heights.astype("f4").astype(float))
    ):
        raise ValueError("one finite exact Float32 height per joint region required")
    return [
        {**plan, "prior_height_m": plan["height_m"], "height_m": float(h)}
        for plan, h in zip(plans, heights, strict=True)
    ]


def recipe_levels(plans, data, recipe):
    if recipe.get("junction_height_mode") in ("joint", "quantized", "rounded_joint"):
        return bound_levels(plans, data.get("joint_level_heights_m"))
    if "joint_level_heights_m" in data:
        raise ValueError("undeclared free joint level heights")
    return plans


def fit_joint_levels(surface, frames, plans, budget):
    from outshine_ref.contracts import Mesh
    from outshine_ref.transport import HeightBudget, HeightFit

    if not isinstance(surface, Mesh) or not isinstance(budget, HeightBudget):
        raise TypeError("owned surface and explicit joint height budget required")
    if len(surface.vertices) > min(budget.max_vertices, 4096) or len(
        surface.triangles
    ) > min(budget.max_faces, 8192):
        return HeightFit(
            None, {"reason": "input_budget_exhausted", "valid": False}
        ), None
    if budget.time_limit_s == 0:
        return HeightFit(
            None, {"reason": "time_budget_exhausted", "valid": False}
        ), None
    from outshine_ref.transport.level_heights._continuous import fit_groups
    from outshine_ref.transport.level_heights._input import checked_groups

    return fit_groups(
        surface,
        frames,
        checked_groups(
            level_groups(surface.vertices[:, :2], plans), len(surface.vertices)
        ),
        budget,
    )
