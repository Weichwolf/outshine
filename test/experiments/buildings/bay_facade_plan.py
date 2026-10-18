"""Coherent ribbons and paired storey/column bays on unchanged wall/opening plans."""

from dataclasses import dataclass, replace

import numpy as np
import shapely
from bay_wall_domains import domains
from shapely.affinity import translate
from terraced_facade_prepare import positions

PROGRAMMES = ("ribbon-fields", "framed-bays")
FIELD_MATERIAL = 60005
RIM_MATERIAL = 60006
BACK_M = 0.005


@dataclass(frozen=True)
class PanelLayer:
    identifier: str
    region_wkb: bytes
    protected_wkb: bytes
    origin_xy: tuple
    tangent: tuple
    base_m: float
    depth_m: float
    material: int

    @property
    def region(self):
        return shapely.from_wkb(self.region_wkb)

    def triangles(self, mode):
        if mode not in ("front", "outer", "closed"):
            raise ValueError("Panel requires front, outer or closed product")
        total = 0
        for polygon in shapely.get_parts(self.region):
            n = (
                len(polygon.exterior.coords)
                - 1
                + sum(len(r.coords) - 1 for r in polygon.interiors)
            )
            front = n + 2 * len(polygon.interiors) - 2
            total += front if mode == "front" else front + 2 * n
            if mode == "closed":
                total += front
        return total


@dataclass(frozen=True)
class BayCell:
    identifier: str
    plane_id: str
    wall_ids: tuple
    window_ids: tuple
    layers: tuple


@dataclass(frozen=True)
class BayPlan:
    source_id: str
    programme: str
    cells: tuple
    rejected: tuple


def layers(prepared, wall, identifier, rectangle, reserved, programme):
    domain = shapely.from_wkb(wall.region_wkb)
    clipped = rectangle.intersection(domain)
    pieces = [("field", clipped.difference(reserved), 0.03, FIELD_MATERIAL)]
    if programme == "framed-bays":
        rim = rectangle.difference(rectangle.buffer(-0.16, join_style="mitre"))
        pieces.append(
            ("rim", rim.intersection(domain).difference(reserved), 0.20, RIM_MATERIAL)
        )
    result = []
    tangent = np.asarray(wall.tangent)
    normal = np.array([tangent[1], -tangent[0]])
    for role, polygon, depth, material in pieces:
        polygon = shapely.orient_polygons(polygon)
        if polygon.is_empty or polygon.area < 0.04:
            raise ValueError("No complete positive-area facade layer after protection")
        if polygon.geom_type not in ("Polygon", "MultiPolygon") or not polygon.is_valid:
            raise ValueError("Facade layer has an invalid clipped profile")
        u, z, _u1, _z1 = polygon.bounds
        polygon = translate(polygon, xoff=-u, yoff=-z)
        origin = np.asarray(wall.origin_xy) + u * tangent - BACK_M * normal
        world = positions(np.array([[*origin, 0.0]]), prepared.mass.core)[0, :2]
        core = prepared.mass.core
        world_tangent = tangent[0] * np.asarray(core.across_xy) + tangent[
            1
        ] * np.asarray(core.along_xy)
        result.append(
            PanelLayer(
                identifier + "/" + role,
                polygon.wkb,
                translate(reserved, xoff=-u, yoff=-z).wkb,
                tuple(world),
                tuple(world_tangent),
                z,
                depth + BACK_M,
                material,
            )
        )
    return tuple(result)


def candidates(wall, programme):
    groups = {}
    origin, tangent = np.asarray(wall.origin_xy), np.asarray(wall.tangent)
    for window in wall.windows:
        key = (
            (window.storey,)
            if programme == "ribbon-fields"
            else (window.storey // 2, window.station // 2)
        )
        groups.setdefault(key, []).append(window)
    for key, windows in sorted(groups.items()):
        pad = 0.18 if programme == "ribbon-fields" else 0.32
        low = min(w.bottom_m for w in windows) - pad
        high = max(w.bottom_m + w.height_m for w in windows) + pad
        u0 = (
            min(
                float((np.asarray(w.centre) - origin) @ tangent) - w.width_m / 2
                for w in windows
            )
            - pad
        )
        u1 = (
            max(
                float((np.asarray(w.centre) - origin) @ tangent) + w.width_m / 2
                for w in windows
            )
            + pad
        )
        yield key, tuple(windows), shapely.box(u0, low, u1, high)


def plan_bays(prepared, programme, *, portals=(), max_triangles=32768):
    if programme not in PROGRAMMES:
        raise ValueError("Unknown facade bay programme")
    if type(max_triangles) is not int or max_triangles < 0:
        raise ValueError("Facade capacity requires a nonnegative integer")
    if len({w.identifier for w in prepared.facade.windows}) != len(
        prepared.facade.windows
    ):
        raise ValueError("Facade bay windows require unique identities")
    prepared = replace(
        prepared,
        facade=replace(
            prepared.facade,
            windows=tuple(sorted(prepared.facade.windows, key=lambda w: w.identifier)),
        ),
    )
    portals = tuple(sorted(portals, key=lambda p: p.identifier))
    walls, rejected = domains(
        prepared,
        portals=portals,
        outward_m=0.03 if programme == "ribbon-fields" else 0.20,
    )
    cells, rejected, total = [], list(rejected), 0
    for wall in walls:
        reserved = shapely.from_wkb(wall.protected_wkb)
        for key, windows, rectangle in candidates(wall, programme):
            identifier = (
                wall.identifier + "/" + programme + "/" + "/".join(map(str, key))
            )
            try:
                if len(windows) < 2:
                    raise ValueError("Fewer than two existing windows in a facade cell")
                items = layers(
                    prepared, wall, identifier, rectangle, reserved, programme
                )
                count = sum(item.triangles("closed") for item in items)
                if total + count > max_triangles:
                    raise ValueError(
                        "Whole facade programme exceeds closed reference capacity"
                    )
                total += count
                cells.append(
                    BayCell(
                        identifier,
                        wall.identifier,
                        wall.wall_ids,
                        tuple(w.identifier for w in windows),
                        items,
                    )
                )
            except ValueError as error:
                rejected.append((identifier, str(error)))
    return BayPlan(prepared.mass.identifier, programme, tuple(cells), tuple(rejected))
