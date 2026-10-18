"""Explicit geometric ground crossing without an invented original source node."""

import numpy as np
import shapely
from stair_module_inputs import reference_profiles


def paving_crossing(state, recipe, sources, chosen):
    key, old_key = chosen["source_id"], chosen["cached_source_id"]
    source, old = sources[key], sources[old_key]
    if (
        chosen.get("kind") != "minor_footway_overlap"
        or recipe.get("source_profiles", {}).get(key) != "unclassified"
        or "road_axis" in recipe
    ):
        raise ValueError("geometric crossing needs its explicit original minor profile")
    root_key = recipe["port"]["source_id"]
    if root_key == old_key or sources[root_key]["properties"].get("class") != "minor":
        raise ValueError("geometric crossing needs its distinct actual minor root")
    row = state["cached_module_rows"][old_key]
    if not np.array_equal(row["xy"], old["xy_m"]):
        raise ValueError("crossed footway axis differs from its immutable source")
    width = row.get("width_m")
    if width != reference_profiles()["footway"]["widthM"]:
        raise ValueError("geometric crossing needs its verified whole footway width")
    axes = []
    for selected in (source, old):
        xy = np.asarray(selected["xy_m"], float)
        if (
            xy.ndim != 2
            or xy.shape[1] != 2
            or not 2 <= len(xy) <= 256
            or not np.isfinite(xy).all()
        ):
            raise ValueError("bounded finite original crossing axes required")
        line = shapely.LineString(xy)
        if not line.is_simple or line.length == 0:
            raise ValueError("simple original crossing axes required")
        axes.append(line)
    point = axes[0].intersection(axes[1])
    if not axes[0].crosses(axes[1]) or point.geom_type != "Point":
        raise ValueError("one actual transverse original-axis intersection required")
    return {
        "walk_width_m": width,
        "procedural_axis_crossing_xy_m": list(point.coords[0]),
    }
