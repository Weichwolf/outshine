"""Retained closed deck/guard/coping components and shared optional portal frames."""

from dataclasses import dataclass

import numpy as np
import shapely
from building_plan import FLOOR_M
from opening_details import expand
from outshine_ref.buildings.modelling import PrismVolume
from outshine_ref.buildings.modelling.prism_union import frozen, primitive_mesh
from outshine_ref.buildings.rooflight_panels import triangle_surface
from terraced_facade_prepare import positions
from terraced_facade_probe import facade_draw
from terraced_facade_products import FacadeProducts
from terraced_facade_surface import joined, polygon_parts
from terraced_terrace_frame import resolved
from terraced_terrace_plan import GUARD_HEIGHT_M


@dataclass(frozen=True)
class Component:
    source: object
    mesh: object


def components(plan):
    values = []
    for t in plan.terraces:
        deck, guard = (
            resolved(shapely.from_wkb(t.deck_wkb)),
            resolved(shapely.from_wkb(t.guard_wkb)),
        )
        coping = guard.buffer(0.02, join_style="mitre").intersection(deck)
        coping = resolved(shapely.union_all(tuple(polygon_parts(coping))))
        sources = (
            PrismVolume(
                t.identifier + "/deck", deck, t.floor_m - FLOOR_M, t.floor_m, 4
            ),
            PrismVolume(
                t.identifier + "/guard",
                guard,
                t.floor_m,
                t.floor_m + GUARD_HEIGHT_M - 0.045,
                12,
            ),
            PrismVolume(
                t.identifier + "/coping",
                coping,
                t.floor_m + GUARD_HEIGHT_M - 0.045,
                t.floor_m + GUARD_HEIGHT_M,
                10,
            ),
        )
        for source in sources:
            if source.polygon.is_empty:
                continue
            points, faces = primitive_mesh(source)
            mesh = {
                "vertices": points,
                "faces": faces.astype(np.uint32),
            }
            mesh["materials"] = np.full(
                len(mesh["faces"]), source.material, dtype=np.uint16
            )
            values.append(Component(source, {k: frozen(v) for k, v in mesh.items()}))
    return tuple(values)


def portal_frames(plan):
    mesh = expand(
        [
            {
                "id": p.identifier,
                "kind": "terrace",
                "centre": p.centre,
                "tangent": p.tangent,
                "bottom_z_m": p.bottom_m,
                "width_m": p.width_m,
                "height_m": p.height_m,
                "storey": p.storey,
                "sill": False,
            }
            for p in plan.portals
        ]
    )
    mesh["vertices"] = positions(mesh["vertices"], plan.prepared.mass.core)
    result = triangle_surface(mesh, 10)
    result["materials"] = mesh["materials"]
    return result


class TerraceProducts:
    def __init__(self, plan):
        self.plan = plan
        self.facade = FacadeProducts(plan.prepared, portals=plan.portals)
        self.components = components(plan)
        self.additions = tuple(
            triangle_surface(c.mesh, c.source.material) for c in self.components
        )
        self.portal_frames = portal_frames(plan)

    def draw(self, near):
        body = (
            facade_draw(self.facade.physical(), self.plan.prepared.mass)
            if near
            else self.facade.coarse.mesh
        )
        meshes = (
            (body, *self.additions)
            if near
            else (body, *self.additions, self.portal_frames)
        )
        return joined(meshes)
