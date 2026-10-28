"""Contained recessed bay volumes and distinct exterior/inner door apertures."""

from dataclasses import dataclass

import numpy as np
import shapely
from building_layout import WALL_M, rectangle
from building_plan import CEILING_M, FLOOR_M
from outshine_ref.buildings.modelling import PrismVolume


@dataclass(frozen=True)
class Aperture:
    identifier: str
    storey: int
    edge: tuple
    centre: tuple
    tangent: tuple
    bottom_m: float
    width_m: float
    height_m: float
    cutter_wkb: bytes


@dataclass(frozen=True)
class Recess:
    identifier: str
    source_window: object
    outer: Aperture
    door: Aperture
    pocket_wkb: bytes
    deck_wkb: bytes
    volumes: tuple
    core_route_wkb: bytes = b""
    door_route_wkb: bytes = b""


def shape(mass, window, recipe):
    storey = mass.storeys[window.storey]
    floor = storey.bottom_m + FLOOR_M
    soffit = storey.top_m - CEILING_M
    if soffit - floor < recipe.door_height_m + 0.045:
        raise ValueError("recess_door_header_clearance")
    polygon = shapely.from_wkb(storey.footprint_wkb)
    interior = polygon.buffer(-WALL_M, join_style="mitre")
    centre, tangent = np.asarray(window.centre), np.asarray(window.tangent)
    inward = np.array([-tangent[1], tangent[0]])
    pocket = rectangle(
        centre, tangent, window.width_m + 2 * WALL_M, 0, recipe.depth_m + WALL_M
    )
    if not polygon.buffer(1e-8).covers(pocket):
        raise ValueError("recess_pocket_leaves_source")
    core = shapely.from_wkb(mass.core.footprint_wkb)
    if pocket.buffer(0.31).intersects(core):
        raise ValueError("recess_reserved_core")
    air = rectangle(centre, tangent, window.width_m, 0.08, recipe.depth_m)
    walls = pocket.difference(air).intersection(interior)
    if walls.is_empty or not walls.is_valid:
        raise ValueError("recess_missing_return_walls")
    identifier = window.identifier + "/recess"
    door_centre = centre + inward * (recipe.depth_m + WALL_M / 2)
    door_cut = rectangle(
        door_centre, tangent, recipe.door_width_m, WALL_M / 2 + 0.02, WALL_M / 2 + 0.02
    )
    guard = rectangle(centre + inward * 0.06, tangent, window.width_m, 0.06, 0.06)
    volumes = (
        PrismVolume(
            identifier + "/lower-returns",
            walls.difference(door_cut),
            floor,
            floor + recipe.door_height_m,
            12,
        ),
        PrismVolume(
            identifier + "/header", walls, floor + recipe.door_height_m, soffit, 12
        ),
        PrismVolume(identifier + "/guard", guard, floor, floor + 1.10, 12),
    )
    outer = Aperture(
        identifier + "/outer-opening",
        window.storey,
        window.edge,
        window.centre,
        window.tangent,
        floor,
        window.width_m,
        soffit - floor,
        rectangle(centre, tangent, window.width_m, 0.08, WALL_M + 0.08).wkb,
    )
    door = Aperture(
        identifier + "/door",
        window.storey,
        window.edge,
        tuple(centre + inward * recipe.depth_m),
        window.tangent,
        floor,
        recipe.door_width_m,
        recipe.door_height_m,
        door_cut.wkb,
    )
    deck = air.intersection(polygon).difference(guard)
    return Recess(identifier, window, outer, door, pocket.wkb, deck.wkb, volumes)
