"""Actual local arrival and whole U-guard height intervals, without a native controller claim."""

from itertools import pairwise

import numpy as np
from entrance_clearance import clearance, collision, section
from outshine_ref.buildings.interiors.geometry import rectangle


def inspect(current, parts):
    result = clearance(
        current["doorway"], parts, floor_z_m=current["floor_z_m"], outside_m=0.7
    )
    solid = collision(parts)
    vertices = np.asarray(solid.to_mesh64().vert_properties)
    scaled = solid.scale((1000.0,) * 3)
    prototype = current["prototype"]
    door = current["doorway"]["door"]
    interior = rectangle(
        door["centre"],
        door["tangent"],
        prototype.width_m - 2 * prototype.guard_m,
        prototype.depth_m - prototype.guard_m,
        0.0,
    )
    guard = current["footprint"].difference(interior)
    floor = current["floor_z_m"]
    samples = np.unique(np.r_[vertices[:, 2], floor, floor + prototype.guard_height_m])
    tested = 0
    for low, high in pairwise(samples):
        a, b = (
            max(float(low), floor),
            min(float(high), floor + prototype.guard_height_m),
        )
        if a < b:
            if guard.difference(section(scaled, (a + b) / 2)).area > 1e-8:
                raise ValueError("Actual balcony guard is incomplete")
            tested += 1
    return result | {
        "guard_intervals": tested,
        "floor_z_m": floor,
        "scope": "Continuous supported local arrival and whole solid guard; no complete upper-storey/transport/native admission",
    }
