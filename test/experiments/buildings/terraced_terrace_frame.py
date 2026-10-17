"""Original world deck polygons, canonical local portal cuts; no polygon repair."""

from dataclasses import replace

import numpy as np
import shapely
from outshine_ref.buildings.interiors.routes import routes
from outshine_ref.buildings.modelling import PrismVolume
from shapely.affinity import affine_transform
from terraced_facade_prepare import positions, world_facade
from terraced_mass_frame import placed_cut


def resolved(polygon):
    value = shapely.set_precision(polygon, 1e-10)
    value = value.buffer(-1e-6, join_style="mitre").buffer(1e-6, join_style="mitre")
    if value.symmetric_difference(polygon).area > 1e-7:
        raise ValueError("Contact resolution changed more than1e-7 square metres")
    return value


def world_prepared(prepared):
    mass, local = prepared.mass, prepared.local
    core = mass.core
    matrix = [
        core.across_xy[0],
        core.along_xy[0],
        core.across_xy[1],
        core.along_xy[1],
        *core.origin_xy,
    ]
    front = affine_transform(shapely.from_wkb(local.core.front_wkb), matrix)
    world = replace(
        mass,
        core=replace(
            core,
            path=tuple(map(tuple, positions(np.asarray(local.core.path), core))),
            front_wkb=front.wkb,
        ),
    )
    cuts = tuple(
        placed_cut(
            PrismVolume(
                w.identifier,
                shapely.from_wkb(w.cutter_wkb),
                w.bottom_m,
                w.bottom_m + w.height_m,
            ),
            core,
        )
        for w in prepared.facade.windows
    )
    return replace(prepared, local=world, facade=world_facade(prepared, cuts))


def local_portal(portal, core):
    across, along, origin = map(
        np.asarray, (core.across_xy, core.along_xy, core.origin_xy)
    )
    matrix = [*across, *along, -origin @ across, -origin @ along]

    def local(wkb):
        return affine_transform(shapely.from_wkb(wkb), matrix).wkb

    return replace(
        portal,
        centre=tuple(
            (np.asarray(portal.centre) - origin) @ np.stack((across, along)).T
        ),
        tangent=tuple(np.asarray(portal.tangent) @ np.stack((across, along)).T),
        cutter_wkb=local(portal.cutter_wkb),
        path_wkb=local(portal.path_wkb),
        core_route_wkb=local(portal.core_route_wkb),
    )


def core_route(space, root, target, core):
    across, along, origin = map(
        np.asarray, (core.across_xy, core.along_xy, core.origin_xy)
    )
    matrix = [*across, *along, -origin @ across, -origin @ along]
    local = affine_transform(space, matrix)
    if not local.is_valid:
        return None
    anchors = (np.asarray([root, target]) - origin) @ np.stack((across, along)).T
    for method in ("doglegs", "graph"):
        paths, _work = routes(local, anchors, method)
        if paths:
            forward = [across[0], along[0], across[1], along[1], *origin]
            return affine_transform(paths[0], forward)
    return None
