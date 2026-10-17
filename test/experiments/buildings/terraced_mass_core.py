"""A source-contained shared stair reservation before detailed room subdivision."""

from dataclasses import dataclass

import numpy as np
import shapely
from shapely.affinity import affine_transform

from building_core import placements
from building_layout import WALL_M, frame
from building_plan import FLOOR_M
from building_stair import core_layout


@dataclass(frozen=True)
class Core:
    identifier: str
    footprint_wkb: bytes
    opening_wkb: bytes
    front_wkb: bytes
    path: tuple
    rise_m: float
    risers: int
    riser_m: float
    flip: bool


def reserve(identifier, polygon, profiles, door, count, rise, bottom):
    top = profiles[-1].buffer(-WALL_M, join_style="mitre")
    if top.geom_type != "Polygon" or top.is_empty:
        raise ValueError("Upper mass has no connected interior for a stair core")
    forward, reverse = frame(polygon)
    local = affine_transform(top, forward)
    tangent = np.asarray(door["tangent"])
    inward = np.array([-tangent[1], tangent[0]])
    entry = shapely.Point(np.asarray(door["centre"]) + inward * 1.0)
    choices, costs = placements(local, affine_transform(entry, forward), count, rise)
    for candidate in choices:
        core = affine_transform(candidate["polygon"], reverse)
        if any(
            not p.buffer(-WALL_M, join_style="mitre").covers(core) for p in profiles
        ):
            continue
        for flip in (False, True):
            layout = core_layout(core, rise, bottom + FLOOR_M, flip=flip)
            if layout is None:
                continue
            if not core.buffer(1e-7).covers(layout["opening"]):
                raise ValueError("Stair opening leaves the prepared core")
            return Core(
                identifier + "/core",
                core.wkb,
                layout["opening"].wkb,
                layout["front_landing"].wkb,
                tuple(map(tuple, layout["path"])),
                rise,
                layout["risers"],
                layout["riser_m"],
                flip,
            ), costs
    raise ValueError("No common contained stair reservation in all mass profiles")
