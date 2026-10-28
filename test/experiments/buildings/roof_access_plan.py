"""Opt-in highest-roof approach within an existing source-height reserve."""

from dataclasses import dataclass, replace

import numpy as np
import shapely
from building_layout import AGENT_RADIUS_M, DOOR_M, WALL_M
from building_plan import AGENT_HEIGHT_M, FLOOR_M
from building_stair import core_layout
from outshine_ref.buildings.modelling import PrismVolume
from terraced_crown_plan import ROOF_MATERIAL, RoofPiece
from terraced_terrace_plan import ACTOR_RADIUS_M, GUARD_HEIGHT_M

CAP_M = 0.08
DOOR_HEIGHT_M = 2.10


@dataclass(frozen=True)
class RoofAccess:
    identifier: str
    opening_wkb: bytes
    house_wkb: bytes
    door_wkb: bytes
    route_xyz: tuple
    bottom_m: float
    rise_m: float
    clear_height_m: float


def with_access(plan):
    """Return an explicit fallback reason without altering a refused body/crown."""
    if plan.access is not None:
        return plan
    crown = plan.crown
    if crown is None:
        return replace(plan, reason="roof access unserved: no reserved crown")
    clear = plan.source_top_m - CAP_M - crown.deck_m
    if clear < max(AGENT_HEIGHT_M, DOOR_HEIGHT_M) or crown.guard_m < GUARD_HEIGHT_M:
        return replace(
            plan, reason="roof access unserved: insufficient height/guard reserve"
        )
    body = plan.body
    core = shapely.box(0, 0, *body.core.size_m)
    house = core.buffer(WALL_M, join_style="mitre")
    inner = shapely.from_wkb(crown.domain_wkb).buffer(-0.24, join_style="mitre")
    if not inner.covers(house):
        return replace(plan, reason="roof access unserved: headhouse leaves inner roof")
    bottom = body.storeys[-1].bottom_m + FLOOR_M
    layout = core_layout(core, crown.deck_m - bottom, bottom, risers=body.core.risers)
    if layout is None:
        return replace(plan, reason="roof access unserved: final stair cannot fit core")
    arrival = layout["path"][-1]
    width = body.core.size_m[0]
    centre = float(np.clip(arrival[0], DOOR_M / 2 + 0.05, width - DOOR_M / 2 - 0.05))
    door = shapely.box(centre - DOOR_M / 2, -WALL_M, centre + DOOR_M / 2, 0)
    route = shapely.LineString([arrival[:2], (centre, -WALL_M - AGENT_RADIUS_M - 0.05)])
    deck = inner.difference(house)
    if not deck.buffer(-AGENT_RADIUS_M).covers(shapely.Point(route.coords[-1])):
        return replace(plan, reason="roof access unserved: no contained roof arrival")
    wall = house.difference(core)
    free = inner.difference(wall.difference(door))
    if not free.covers(route.buffer(ACTOR_RADIUS_M, quad_segs=32)):
        return replace(
            plan, reason="roof access unserved: doorway route lacks clearance"
        )
    opening = layout["opening"]
    identifier = body.identifier + "/roof-access"
    access = RoofAccess(
        identifier,
        opening.wkb,
        house.wkb,
        door.wkb,
        tuple(map(tuple, layout["path"]))
        + tuple((x, y, crown.deck_m) for x, y in list(route.coords)[1:]),
        bottom,
        crown.deck_m - bottom,
        clear,
    )
    pieces = tuple(p for p in crown.parts if p.role not in ("service", "service-cap"))
    pieces = tuple(
        replace(
            p, volume=replace(p.volume, polygon=p.volume.polygon.difference(opening))
        )
        if p.role == "slab"
        else p
        for p in pieces
    )
    definitions = (
        (
            "access-wall",
            wall.difference(door),
            crown.deck_m,
            plan.source_top_m - CAP_M,
            12,
        ),
        (
            "access-header",
            wall.intersection(door),
            crown.deck_m + DOOR_HEIGHT_M,
            plan.source_top_m - CAP_M,
            12,
        ),
        (
            "access-cap",
            house,
            plan.source_top_m - CAP_M,
            plan.source_top_m,
            ROOF_MATERIAL,
        ),
    )
    pieces += tuple(
        RoofPiece(
            role, PrismVolume(identifier + "/" + role, polygon, low, high, material)
        )
        for role, polygon, low, high, material in definitions
    )
    updated = replace(
        crown,
        parts=pieces,
        kind="parapet-access",
        deck_wkb=shapely.from_wkb(crown.deck_wkb)
        .union(
            shapely.union_all(
                [p.volume.polygon for p in crown.parts if p.role == "service"]
            )
        )
        .difference(wall.union(opening))
        .wkb,
    )
    return replace(
        plan,
        crown=updated,
        access=access,
        reason="reserved core reaches highest roof through a physical headhouse",
    )
