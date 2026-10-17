"""One core-local CSG frame, with a single final placement of complete products."""

from dataclasses import replace
from types import MappingProxyType

import numpy as np
import shapely
from shapely.affinity import affine_transform

from building_plan import FLOOR_M
from building_stair import core_layout
from outshine_ref.buildings.modelling.prism_union import frozen


def local_plan(plan):
    core = plan.core
    origin, across, along = map(
        np.asarray, (core.origin_xy, core.across_xy, core.along_xy)
    )
    inverse = [*across, *along, -origin @ across, -origin @ along]

    def geometry(wkb):
        return affine_transform(shapely.from_wkb(wkb), inverse).wkb

    def point(value):
        return tuple((np.asarray(value) - origin) @ np.array([across, along]).T)

    rectangle = shapely.box(0, 0, *core.size_m)
    layout = core_layout(rectangle, core.rise_m, plan.bottom_m + FLOOR_M)
    if layout is None:
        raise ValueError("Canonical core cannot accommodate the planned stair")
    local_core = replace(
        core,
        footprint_wkb=rectangle.wkb,
        opening_wkb=layout["opening"].wkb,
        front_wkb=layout["front_landing"].wkb,
        path=tuple(map(tuple, layout["path"])),
        flip=False,
        approach_wkb=geometry(core.approach_wkb),
        origin_xy=(0.0, 0.0),
        across_xy=(1.0, 0.0),
        along_xy=(0.0, 1.0),
    )
    tangent = np.asarray(plan.entry.tangent) @ np.array([across, along]).T
    entry = replace(
        plan.entry,
        centre=point(plan.entry.centre),
        tangent=tuple(tangent),
        opening_wkb=geometry(plan.entry.opening_wkb),
        landing_wkb=geometry(plan.entry.landing_wkb),
    )
    return replace(
        plan,
        source_wkb=geometry(plan.source_wkb),
        storeys=tuple(
            replace(
                s,
                footprint_wkb=geometry(s.footprint_wkb),
                terrace_wkb=geometry(s.terrace_wkb),
            )
            for s in plan.storeys
        ),
        core=local_core,
        entry=entry,
    )


def placed(product, core):
    mesh = dict(product.mesh)
    for key in ("vertices", "normals"):
        if key not in mesh:
            continue
        values = np.array(mesh[key], copy=True)
        values[:, :2] = values[:, :1] * np.asarray(core.across_xy) + values[
            :, 1:2
        ] * np.asarray(core.along_xy)
        if key == "vertices":
            values[:, :2] += core.origin_xy
        mesh[key] = values
    return replace(
        product, mesh=MappingProxyType({k: frozen(v) for k, v in mesh.items()})
    )


def placed_cut(cut, core):
    across, along = np.asarray(core.across_xy), np.asarray(core.along_xy)
    matrix = [across[0], along[0], across[1], along[1], *core.origin_xy]
    return replace(cut, polygon=affine_transform(cut.polygon, matrix))
