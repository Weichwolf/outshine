"""A source-contained shared stair reservation before detailed room subdivision."""

import math
from dataclasses import dataclass

import numpy as np
import shapely
from building_core import placements
from building_layout import AGENT_RADIUS_M, WALL_M, frame
from building_plan import FLOOR_M
from building_stair import core_layout, geometry_frame
from outshine_ref.buildings.interiors.routes import routes
from shapely.affinity import affine_transform

CLEARANCE_RESERVE_M = 0.001


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
    approach_wkb: bytes
    approach_method: str
    origin_xy: tuple
    across_xy: tuple
    along_xy: tuple
    size_m: tuple


def canonical_layout(core, rise, bottom, flip):
    origin, along, across, measured, _reverse = geometry_frame(core, flip)
    size = np.round(measured, 8)
    if np.max(abs(size - measured)) > 1e-7:
        raise ValueError("Core dimensions exceed their metric tolerance")
    layout = core_layout(shapely.box(0, 0, *size), rise, bottom + FLOOR_M)
    if layout is None:
        return None
    matrix = [across[0], along[0], across[1], along[1], *origin]
    world = dict(layout)
    for key in ("opening", "front_landing"):
        world[key] = affine_transform(layout[key], matrix)
    path = np.array(layout["path"], copy=True)
    path[:, :2] = origin + path[:, :1] * across + path[:, 1:2] * along
    world["path"] = path
    return world, origin, across, along, size


def circulation_space(polygon, core, front):
    radius = AGENT_RADIUS_M / math.cos(math.pi / (4 * 32)) + CLEARANCE_RESERVE_M
    blocked = core.difference(front).buffer(radius, quad_segs=32)
    return polygon.buffer(-(WALL_M + radius), join_style="mitre").difference(blocked)


def approach(profiles, core, layout, entry):
    spaces = [circulation_space(p, core, layout["front_landing"]) for p in profiles]
    target = np.asarray(layout["path"][0, :2])
    if any(
        s.geom_type != "Polygon" or s.is_empty or not s.covers(shapely.Point(target))
        for s in spaces
    ):
        return None
    for method in ("doglegs", "graph"):
        paths, _work = routes(spaces[0], [entry, target], method)
        if paths:
            return paths[0], method
    return None


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
            canonical = canonical_layout(core, rise, bottom, flip)
            if canonical is None:
                continue
            layout, origin, across, along, size = canonical
            if not core.buffer(1e-7).covers(layout["opening"]):
                raise ValueError("Stair opening leaves the prepared core")
            access = approach(
                profiles, core, layout, np.asarray(door["centre"]) + inward * 1.5
            )
            if access is None:
                continue
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
                access[0].wkb,
                access[1],
                tuple(origin),
                tuple(across),
                tuple(along),
                tuple(size),
            ), costs
    raise ValueError("No common contained stair reservation in all mass profiles")
