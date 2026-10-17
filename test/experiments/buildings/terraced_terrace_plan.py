"""Protected setback decks and source-axis portals before physical expansion."""

import math
from dataclasses import dataclass, replace

import numpy as np
import shapely
from building_layout import WALL_M, rectangle
from building_plan import FLOOR_M
from terraced_facade_prepare import prepare
from terraced_facade_surface import polygon_parts
from terraced_terrace_frame import (
    core_route,
    local_portal,
    resolved,
    world_interior,
    world_prepared,
)

GUARD_M = 0.12
GUARD_HEIGHT_M = 1.10
PORTAL_WIDTH_M = 1.20
PORTAL_HEIGHT_M = 2.15
ACTOR_RADIUS_M = 0.30 / math.cos(math.pi / (4 * 32))
CLEARANCE_RESERVE_M = 0.001


@dataclass(frozen=True)
class Portal:
    identifier: str
    storey: int
    edge: tuple
    centre: tuple
    tangent: tuple
    bottom_m: float
    height_m: float
    width_m: float
    cutter_wkb: bytes
    replaced_window_id: str
    path_wkb: bytes
    core_route_wkb: bytes


@dataclass(frozen=True)
class Terrace:
    identifier: str
    storey: int
    deck_wkb: bytes
    guard_wkb: bytes
    floor_m: float
    portal: object
    reason: object


@dataclass(frozen=True)
class TerracePlan:
    prepared: object
    terraces: tuple
    portals: tuple
    replaced_window_ids: tuple


def portal_for(prepared, index, terrace, guard, interior):
    mass = prepared.local
    storey = mass.storeys[index]
    core = shapely.from_wkb(mass.core.footprint_wkb)
    front = shapely.from_wkb(mass.core.front_wkb)
    root = np.asarray(mass.core.path[0][:2])
    space = interior.difference(core.difference(front)).buffer(
        -(ACTOR_RADIUS_M + CLEARANCE_RESERVE_M), quad_segs=32
    )
    choices = sorted(
        (w for w in prepared.facade.windows if w.storey == index),
        key=lambda w: (np.linalg.norm(np.asarray(w.centre) - root), w.identifier),
    )
    for w in choices:
        centre, tangent = np.asarray(w.centre), np.asarray(w.tangent)
        normal = np.array([tangent[1], -tangent[0]])
        outside = rectangle(centre, tangent, 1.40, 1.05, 0)
        if outside.difference(terrace.difference(guard)).area > 1e-8:
            continue
        inside = centre - normal * 0.85
        line = shapely.LineString([inside, centre + normal * 0.70])
        opening = rectangle(centre, tangent, PORTAL_WIDTH_M, 0.08, WALL_M + 0.08)
        free = interior.union(terrace).union(opening).difference(guard)
        if not free.buffer(-ACTOR_RADIUS_M, quad_segs=32).buffer(1e-9).covers(line):
            continue
        access = core_route(space, root, inside, mass.core)
        if access is None:
            continue
        return Portal(
            mass.storeys[index].identifier
            + "/terrace-portal/"
            + w.identifier.rsplit("/", 1)[-1]
            + f"/{w.edge[0]}/{w.edge[1]}",
            index,
            w.edge,
            w.centre,
            w.tangent,
            storey.bottom_m + FLOOR_M,
            PORTAL_HEIGHT_M,
            PORTAL_WIDTH_M,
            opening.wkb,
            w.identifier,
            line.wkb,
            access.wkb,
        )
    return None


def plan(mass):
    prepared = prepare(mass)
    active = tuple(
        i
        for i, s in enumerate(mass.storeys)
        if i and not shapely.from_wkb(s.terrace_wkb).is_empty
    )
    world = world_prepared(prepared, active)
    terraces, portals = [], []
    for index in active:
        storey = mass.storeys[index]
        original_lower = shapely.from_wkb(mass.storeys[index - 1].footprint_wkb)
        previous = resolved(original_lower)
        current = resolved(shapely.from_wkb(storey.footprint_wkb))
        if not previous.is_valid or not current.is_valid:
            raise ValueError("Terrace requires valid source profile operands")
        difference = previous.difference(current)
        interior = world_interior(prepared, index)
        original_area = shapely.from_wkb(mass.storeys[index].terrace_wkb).area
        if abs(difference.area - original_area) > 1e-7:
            raise ValueError("Local terrace changed its original profile area")
        for number, polygon in enumerate(polygon_parts(difference)):
            if polygon.area <= 1e-8:
                continue
            guard = polygon.difference(
                original_lower.buffer(-GUARD_M, join_style="mitre")
            )
            portal = portal_for(world, index, polygon, guard, interior)
            if portal:
                portal = local_portal(portal, mass.core)
            terraces.append(
                Terrace(
                    storey.identifier + f"/terrace/{number}",
                    index,
                    polygon.wkb,
                    guard.wkb,
                    storey.bottom_m + FLOOR_M,
                    portal,
                    None
                    if portal
                    else "no aligned portal with supported actor-clear core/terrace route",
                )
            )
            if portal:
                portals.append(portal)
    removed = tuple(p.replaced_window_id for p in portals)
    facade = replace(
        prepared.facade,
        windows=tuple(
            w for w in prepared.facade.windows if w.identifier not in removed
        ),
    )
    return TerracePlan(
        replace(prepared, facade=facade), tuple(terraces), tuple(portals), removed
    )
