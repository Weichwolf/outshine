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
    route_work: tuple = ()


@dataclass(frozen=True)
class PortalResolution:
    portal: object
    reason: object
    work: tuple = ()
    mesh: object = None


def portal_descriptor(mass, index, window, line, opening, access):
    return Portal(
        mass.storeys[index].identifier
        + "/terrace-portal/"
        + window.identifier.rsplit("/", 1)[-1]
        + f"/{window.edge[0]}/{window.edge[1]}",
        index,
        window.edge,
        window.centre,
        window.tangent,
        mass.storeys[index].bottom_m + FLOOR_M,
        PORTAL_HEIGHT_M,
        PORTAL_WIDTH_M,
        opening.wkb,
        window.identifier,
        line.wkb,
        access.wkb,
    )


def portal_for(
    prepared, index, terrace, guard, interior, *, routing="grid", route_mesh=None
):
    mass = prepared.local
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
    failed = []
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
            if routing == "triangulated":
                failed.append((w, inside, line, opening))
            continue
        return PortalResolution(
            portal_descriptor(mass, index, w, line, opening, access),
            None,
            mesh=route_mesh,
        )
    reason = "no aligned portal with supported actor-clear core/terrace route"
    if not failed:
        return PortalResolution(None, reason, mesh=route_mesh)
    from terraced_route_mesh import CoreRouteMesh

    mesh = CoreRouteMesh(space, root, mass.core) if route_mesh is None else route_mesh
    before = dict(mesh.work)

    def work():
        counts = {
            key: mesh.work[key] - before[key]
            for key in (
                "mesh_builds",
                "route_queries",
                "expanded_triangles",
                "containment_queries",
            )
        }
        counts.update(
            {
                key: mesh.work[key]
                for key in ("input_vertices", "triangles", "shared_edges")
            }
        )
        counts["mesh_reused"] = before["mesh_builds"] > 0
        return tuple(counts.items())

    for w, inside, line, opening in failed:
        try:
            access = mesh.route(inside)
        except (ValueError, shapely.errors.GEOSException) as error:
            mesh.reason = str(error)
            mesh.mesh_failure = mesh.reason
            access = None
        if access is not None:
            return PortalResolution(
                portal_descriptor(mass, index, w, line, opening, access),
                None,
                work(),
                mesh,
            )
    return PortalResolution(None, reason + "; " + str(mesh.reason), work(), mesh)


def plan(mass, *, routing="grid"):
    if routing not in ("grid", "triangulated"):
        raise ValueError("Terrace routing requires grid or triangulated")
    prepared = prepare(mass)
    active = tuple(
        i
        for i, s in enumerate(mass.storeys)
        if i and not shapely.from_wkb(s.terrace_wkb).is_empty
    )
    world = world_prepared(prepared, active)
    terraces, portals, route_work = [], [], []
    for index in active:
        route_mesh = None
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
            selection = portal_for(
                world,
                index,
                polygon,
                guard,
                interior,
                routing=routing,
                route_mesh=route_mesh,
            )
            route_mesh = selection.mesh
            portal = selection.portal
            if selection.work:
                route_work.append(
                    (
                        storey.identifier + f"/terrace/{number}",
                        selection.reason,
                        selection.work,
                    )
                )
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
                    None if portal else selection.reason,
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
        replace(prepared, facade=facade),
        tuple(terraces),
        tuple(portals),
        removed,
        tuple(route_work),
    )
