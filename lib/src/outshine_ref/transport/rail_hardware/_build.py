"""Bounded hardware for an explicitly chosen horizontal rail bed."""

import numpy as np

from ._geometry import SLEEPER, footprints, sleeper_instances, sweep_rails
from ._types import RailBudget, RailHardware, RailRecipe

DEFAULT_RECIPE = RailRecipe()
DEFAULT_BUDGET = RailBudget()


def build_rail_hardware(xy, datum_m, recipe=DEFAULT_RECIPE, budget=DEFAULT_BUDGET):
    xy = np.asarray(xy, dtype=float)
    if xy.ndim != 2 or xy.shape[1] != 2 or len(xy) < 2 or not np.isfinite(xy).all():
        raise ValueError(
            "finite Nx2 metric centreline with at least two stations required"
        )
    if not np.isfinite(datum_m):
        raise ValueError("explicit finite horizontal bed datum required")
    if len(xy) > budget.maximum_stations or 24 * len(xy) > budget.maximum_rail_vertices:
        return None, {"valid": False, "reason": "rail_station_budget"}
    delta = np.diff(xy, axis=0)
    lengths = np.linalg.norm(delta, axis=1)
    if np.any(lengths <= 1e-7):
        return None, {"valid": False, "reason": "rail_zero_length_segment"}
    if np.linalg.norm(xy[-1] - xy[0]) <= 1e-7:
        return None, {"valid": False, "reason": "rail_periodic_join_not_planned"}
    tangent = delta / lengths[:, None]
    if len(tangent) > 1:
        cosine = np.einsum("ij,ij->i", tangent[:-1], tangent[1:])
        if np.any(cosine < np.cos(np.deg2rad(recipe.maximum_turn_deg))):
            return None, {"valid": False, "reason": "rail_horizontal_corner"}
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    miter = np.vstack((normal[0], (normal[:-1] + normal[1:]) / 2, normal[-1]))
    miter[1:-1] /= np.einsum("ij,ij->i", miter[1:-1], normal[:-1])[:, None]
    outer = (recipe.gauge_m + 0.072) / 2 + 0.075
    advance = lengths - outer * abs(
        np.einsum("ij,ij->i", np.diff(miter, axis=0), tangent)
    )
    if np.any(advance <= 1e-7):
        return None, {"valid": False, "reason": "rail_offset_fold"}
    total = float(lengths.sum())
    if total < recipe.sleeper_width_m:
        return None, {"valid": False, "reason": "rail_span_too_short"}
    count = (
        int(np.floor((total - recipe.sleeper_width_m) / recipe.sleeper_spacing_m)) + 1
    )
    if count > budget.maximum_sleepers:
        return None, {"valid": False, "reason": "rail_sleeper_budget"}
    rails = sweep_rails(xy, miter, datum_m, recipe.gauge_m, recipe.sleeper_height_m)
    transforms = sleeper_instances(
        xy,
        lengths,
        recipe.sleeper_spacing_m,
        (recipe.sleeper_length_m, recipe.sleeper_width_m, recipe.sleeper_height_m),
        datum_m,
    )
    shapes, keep, audit = footprints(xy, miter, recipe.gauge_m, transforms)
    if shapes is None:
        return None, {"valid": False, **audit}
    if not keep.all():
        transforms = transforms[keep]
    transforms.setflags(write=False)
    product = RailHardware(rails, SLEEPER, transforms, tuple(shapes))
    return product, {
        **audit,
        "valid": True,
        "reason": "",
        "centreline_length_m": total,
        "rail_vertices": len(rails.vertices),
        "rail_triangles": len(rails.triangles),
        "sleepers": len(transforms),
        "sleeper_transform_bytes": transforms.nbytes,
        "shared_sleeper_bytes": SLEEPER.vertices.nbytes + SLEEPER.triangles.nbytes,
        "rail_buffer_bytes": rails.vertices.nbytes + rails.triangles.nbytes,
        "maximum_miter_factor": float(np.linalg.norm(miter, axis=1).max()),
        "minimum_rail_segment_advance_m": float(advance.min()),
        "contact_scope": "Explicit horizontal bed only; caller verifies entire footprints against the selected finite bed. No switch, train-path or C1 horizontal-curve proof.",
    }
