"""Source-axis stacks before expansion; old and new actor routes remain protected."""

import math
from dataclasses import dataclass, replace

import numpy as np
import shapely
from building_layout import WALL_M
from facade_recess_geometry import DOOR_FRAME_TRIANGLES, MAX_TRIANGLES
from facade_recess_shape import shape
from outshine_ref.buildings.modelling.prism_union import triangle_bound
from provider_contract import seed_for
from terraced_route_mesh import CoreRouteMesh
from terraced_terrace_plan import ACTOR_RADIUS_M, CLEARANCE_RESERVE_M


@dataclass(frozen=True)
class RecessRecipe:
    depth_m: float = 1.2
    door_width_m: float = 1.1
    door_height_m: float = 2.1
    cells: int = 12
    stacks: int = 2
    attempts: int = 48

    def __post_init__(self):
        for value in (self.depth_m, self.door_width_m, self.door_height_m):
            if type(value) not in (int, float) or not math.isfinite(value):
                raise ValueError("Recess dimensions require finite metres")
        if not 0.9 <= self.depth_m <= 2.0 or not 0.9 <= self.door_width_m <= 1.2:
            raise ValueError("Recess depth/door-width recipe domain")
        if not 2.0 <= self.door_height_m <= 2.2:
            raise ValueError("Recess door-height recipe domain")
        for value, cap in ((self.cells, 24), (self.stacks, 4), (self.attempts, 96)):
            if type(value) is not int or not 0 < value <= cap:
                raise ValueError("Bounded positive recess capacities required")


@dataclass(frozen=True)
class RecessSelection:
    cells: tuple
    rejected: tuple
    work: tuple


def stacks(prepared):
    grouped = {}
    for window in prepared.facade.windows:
        if not window.storey or window.edge[0] != 0:
            continue
        key = (
            window.edge,
            window.station,
            window.centre,
            window.tangent,
            window.width_m,
            prepared.local.storeys[window.storey].footprint_wkb,
        )
        grouped.setdefault(key, []).append(window)
    ordered = sorted(grouped, key=lambda k: (-len(grouped[k]), k))
    if ordered:
        offset = seed_for(prepared.mass.identifier) % min(8, len(ordered))
        ordered = ordered[offset:] + ordered[:offset]
    return [(key, sorted(grouped[key], key=lambda w: w.storey)) for key in ordered]


def routes(mass, recess, recipe, occupied, pockets, work):
    index = recess.outer.storey
    polygon = shapely.from_wkb(mass.storeys[index].footprint_wkb)
    interior = polygon.buffer(-WALL_M, join_style="mitre")
    core = shapely.from_wkb(mass.core.footprint_wkb)
    front = shapely.from_wkb(mass.core.front_wkb)
    pocket = shapely.from_wkb(recess.pocket_wkb)
    if any(
        pocket.intersects(line.buffer(ACTOR_RADIUS_M + CLEARANCE_RESERVE_M))
        for line in occupied
    ):
        raise ValueError("recess_existing_actor_route")
    inward = np.array([-recess.outer.tangent[1], recess.outer.tangent[0]])
    inside = np.asarray(recess.outer.centre) + inward * (recipe.depth_m + WALL_M + 0.85)
    target = np.asarray(recess.outer.centre) + inward * (recipe.depth_m / 2)
    door_route = shapely.LineString([inside, target])
    tube = door_route.buffer(ACTOR_RADIUS_M + CLEARANCE_RESERVE_M, quad_segs=32)
    if any(
        tube.intersection(v.polygon).area > 1e-8
        for v in recess.volumes
        if v.low_m < recess.door.bottom_m + 1.9
    ):
        raise ValueError("recess_own_door_route_obstructed")
    excluded = shapely.union_all([pocket, *pockets])
    free = interior.difference(excluded).difference(core.difference(front))
    space = free.buffer(-(ACTOR_RADIUS_M + CLEARANCE_RESERVE_M), quad_segs=32)
    mesh = CoreRouteMesh(space, mass.core.path[0][:2], mass.core)
    try:
        route = mesh.route(inside)
    finally:
        work["route_queries"] += mesh.work["route_queries"]
        work["constructed_triangles"] += (
            0 if mesh.triangles is None else len(mesh.triangles)
        )
        work["expanded_triangles"] += mesh.work["expanded_triangles"]
    if route is None:
        raise ValueError("recess_core_route:" + str(mesh.reason))
    return replace(recess, core_route_wkb=route.wkb, door_route_wkb=door_route.wkb)


def plan(terraces, recipe):
    if not isinstance(recipe, RecessRecipe):
        raise TypeError("Explicit immutable RecessRecipe required")
    prepared, accepted, rejected = terraces.prepared, [], []
    protected, pockets = {}, {}
    for portal in terraces.portals:
        protected.setdefault(portal.storey, []).extend(
            shapely.from_wkb(wkb) for wkb in (portal.core_route_wkb, portal.path_wkb)
        )
    work = {
        "attempts": 0,
        "accepted_stacks": 0,
        "route_queries": 0,
        "constructed_triangles": 0,
        "expanded_triangles": 0,
        "estimated_added_triangles": 0,
    }
    for _key, windows in stacks(prepared):
        if work["accepted_stacks"] >= recipe.stacks or len(accepted) >= recipe.cells:
            break
        before = len(accepted)
        for window in windows:
            if work["attempts"] >= recipe.attempts or len(accepted) >= recipe.cells:
                break
            work["attempts"] += 1
            index = window.storey
            try:
                current = shape(prepared.local, window, recipe)
                triangle_estimate = (
                    sum(triangle_bound(v) for v in current.volumes)
                    + DOOR_FRAME_TRIANGLES
                )
                if (
                    work["estimated_added_triangles"] + triangle_estimate
                    > MAX_TRIANGLES
                ):
                    raise ValueError("recess_assembly_input_triangle_budget")
                pocket = shapely.from_wkb(current.pocket_wkb)
                if any(pocket.intersects(p) for p in pockets.get(index, ())):
                    raise ValueError("recess_overlapping_pockets")
                current = routes(
                    prepared.local,
                    current,
                    recipe,
                    protected.get(index, ()),
                    pockets.get(index, ()),
                    work,
                )
            except (ValueError, shapely.errors.GEOSException) as error:
                rejected.append((window.identifier, str(error)))
                continue
            accepted.append(current)
            work["estimated_added_triangles"] += triangle_estimate
            pockets.setdefault(index, []).append(pocket)
            protected.setdefault(index, []).extend(
                shapely.from_wkb(wkb)
                for wkb in (current.core_route_wkb, current.door_route_wkb)
            )
        work["accepted_stacks"] += len(accepted) > before
    return RecessSelection(tuple(accepted), tuple(rejected), tuple(work.items()))
