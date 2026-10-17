"""Simple hollow open floors and real stairs, without detailed room subdivision."""

from dataclasses import dataclass

import manifold3d as manifold
import numpy as np
import shapely

from building_mesh import solid
from building_layout import WALL_M
from building_plan import CEILING_M, FLOOR_M
from building_stair import PLATE_M, core_layout, flight
from outshine_ref.buildings.modelling import PrismVolume, union_prisms
from outshine_ref.buildings.modelling.mesh_fans import connected_fans
from outshine_ref.buildings.modelling.prism_cut import decoded, encoded, subtract_prisms
from terraced_mass_frame import local_plan, placed
from terraced_mass_geometry import physical


@dataclass(frozen=True)
class SurfaceMaterial:
    material: int


@dataclass(frozen=True)
class OpenFloors:
    shell: object
    stairs: object
    landing: object
    cuts: tuple


def cavities(plan):
    values = []
    for i, storey in enumerate(plan.storeys):
        polygon = shapely.from_wkb(storey.footprint_wkb)
        interior = polygon.buffer(-WALL_M, join_style="mitre").intersection(polygon)
        values.append(
            PrismVolume(
                storey.identifier + "/open-floor",
                interior,
                storey.bottom_m + FLOOR_M,
                storey.top_m - CEILING_M,
                12,
            )
        )
        if i:
            values.append(
                PrismVolume(
                    storey.identifier + "/stair-aperture",
                    shapely.from_wkb(plan.core.opening_wkb),
                    storey.bottom_m - CEILING_M,
                    storey.bottom_m + FLOOR_M,
                    12,
                )
            )
    values.append(
        PrismVolume(
            plan.entry.identifier,
            shapely.from_wkb(plan.entry.opening_wkb),
            plan.bottom_m + FLOOR_M,
            plan.bottom_m + FLOOR_M + plan.entry.height_m,
            12,
        )
    )
    return tuple(values)


def stairs(plan):
    core = shapely.from_wkb(plan.core.footprint_wkb)
    template = core_layout(
        core, plan.core.rise_m, plan.bottom_m + FLOOR_M, flip=plan.core.flip
    )
    if template is None or template["opening"].wkb != plan.core.opening_wkb:
        raise ValueError("Actual stair no longer matches its reserved mass/core plan")
    storage = [], [], [], []
    for i in range(len(plan.storeys) - 1):
        shift = i * plan.core.rise_m
        for descriptor in template["flights"]:
            flight(storage, descriptor | {"base_z_m": descriptor["base_z_m"] + shift})
        height = template["mid_z_m"] + shift
        solid(storage, template["mid_landing"], height - PLATE_M, height, 5)
    raw = {
        "vertices": np.asarray(storage[0], dtype=np.float64),
        "faces": np.asarray(storage[1], dtype=np.uint32),
        "materials": np.full(len(storage[1]), 5, dtype=np.uint16),
    }
    groups = np.asarray(storage[3])
    inputs = [
        prepared(raw, np.flatnonzero(groups == part)) for part in np.unique(groups)
    ]
    identifier = manifold.Manifold.reserve_ids(len(inputs))
    origin = np.zeros(3)
    operands = [
        encoded(mesh, roots, origin, identifier + i, ("uv",))
        for i, (mesh, roots) in enumerate(inputs)
    ]
    result = manifold.Manifold.batch_boolean(operands, manifold.OpType.Add)
    if result.status() != manifold.Error.NoError:
        raise ValueError(
            "Actual stair component union rejected: " + str(result.status())
        )
    return decoded(
        result,
        inputs[0][0],
        tuple(SurfaceMaterial(5) for _ in inputs[1:]),
        tuple(plan.core.identifier + f"/component/{i}" for i in range(len(inputs))),
        {identifier + i: i for i in range(len(inputs))},
        [len(mesh["faces"]) for mesh, _roots in inputs],
        origin,
        ("uv",),
        32768,
        (),
    )


def prepared(raw, order=None):
    mesh = dict(raw, uv=np.zeros((len(raw["vertices"]), 2)))
    order = np.arange(len(raw["faces"])) if order is None else order
    points, faces, duplicates, targets = connected_fans(mesh, order)
    roots = np.arange(len(points), dtype=np.uint64)
    roots[duplicates] = targets
    return {
        "vertices": points[:, :3],
        "faces": faces,
        "uv": points[:, 3:5],
        "materials": raw["materials"][order],
    }, roots


def build(plan):
    world = plan
    plan = local_plan(plan)
    cuts = cavities(plan)
    shell = subtract_prisms(
        plan.identifier + "/mass", physical(plan), cuts, max_triangles=32768
    )
    stair_mesh = stairs(plan)
    region = shapely.from_wkb(plan.entry.landing_wkb).difference(
        shapely.from_wkb(plan.source_wkb)
    )
    landing = union_prisms(
        (
            PrismVolume(
                plan.entry.identifier + "/landing",
                region,
                plan.bottom_m,
                plan.bottom_m + FLOOR_M,
                4,
            ),
        ),
        max_triangles=256,
    )
    return OpenFloors(
        *(placed(p, world.core) for p in (shell, stair_mesh, landing)),
        cavities(world),
    )
