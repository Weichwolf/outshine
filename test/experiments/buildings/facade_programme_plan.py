"""Whole-wall plinth/main/attic composition over actual source opening axes."""

from dataclasses import dataclass, replace

import numpy as np
import shapely
from bay_facade_plan import BACK_M, FIELD_MATERIAL, PanelLayer
from bay_wall_domains import domains
from shapely.affinity import translate
from terraced_facade_prepare import positions

PROGRAMMES = ("ribbon-screens", "vertical-slots")
PLINTH, MAIN, ATTIC, FIN = 60007, 60008, 60009, 60010
JOINT_M = 0.04
PROFILE_GRID_M = 1e-6


@dataclass(frozen=True)
class ProgrammeLayer(PanelLayer):
    profile_grid_m: float
    profile_adjustment_m: float


@dataclass(frozen=True)
class ProgrammeCell:
    identifier: str
    plane_id: str
    role: str
    window_ids: tuple
    layers: tuple


@dataclass(frozen=True)
class FacadeProgramme:
    source_id: str
    programme: str
    cells: tuple
    rejected: tuple


def layer(prepared, wall, identifier, polygon, protected, material, depth):
    polygon = shapely.orient_polygons(polygon)
    if (
        polygon.geom_type not in ("Polygon", "MultiPolygon")
        or polygon.is_empty
        or not polygon.is_valid
        or polygon.area < 0.04
    ):
        raise ValueError("Programme layer needs a complete positive polygonal region")
    u, z, _u1, _z1 = polygon.bounds
    local = translate(polygon, xoff=-u, yoff=-z)
    quantized = shapely.orient_polygons(shapely.set_precision(local, PROFILE_GRID_M))
    error = local.hausdorff_distance(quantized)
    if (
        quantized.is_empty
        or quantized.area < 0.04
        or not np.isfinite(error)
        or error > PROFILE_GRID_M
    ):
        raise ValueError(
            "Programme profile cannot meet its micrometre construction grid"
        )
    tangent = np.asarray(wall.tangent)
    normal = np.array([tangent[1], -tangent[0]])
    origin = np.asarray(wall.origin_xy) + u * tangent - BACK_M * normal
    core = prepared.mass.core
    world = positions(np.array([[*origin, 0.0]]), core)[0, :2]
    direction = tangent[0] * np.asarray(core.across_xy) + tangent[1] * np.asarray(
        core.along_xy
    )
    return ProgrammeLayer(
        identifier,
        quantized.wkb,
        translate(protected, xoff=-u, yoff=-z).wkb,
        tuple(world),
        tuple(direction),
        z,
        depth + BACK_M,
        material,
        PROFILE_GRID_M,
        float(error),
    )


def fields(wall, programme):
    groups = {}
    for window in wall.windows:
        key = window.storey if programme == "ribbon-screens" else window.station
        groups.setdefault(key, []).append(window)
    for key, windows in sorted(groups.items()):
        if len(windows) < 2:
            continue
        coordinates = [
            float((np.asarray(w.centre) - wall.origin_xy) @ wall.tangent)
            for w in windows
        ]
        pad = 0.20
        rectangle = shapely.box(
            min(u - w.width_m / 2 for u, w in zip(coordinates, windows, strict=True))
            - pad,
            min(w.bottom_m for w in windows) - pad,
            max(u + w.width_m / 2 for u, w in zip(coordinates, windows, strict=True))
            + pad,
            max(w.bottom_m + w.height_m for w in windows) + pad,
        )
        yield key, tuple(w.identifier for w in windows), rectangle


def wall_cells(prepared, wall, programme):
    domain = shapely.from_wkb(wall.region_wkb)
    protected = shapely.from_wkb(wall.protected_wkb)
    cut_protection = protected.buffer(2 * PROFILE_GRID_M, join_style="mitre")
    cells, field_regions = [], []
    for key, windows, rectangle in fields(wall, programme):
        region = rectangle.intersection(domain).difference(cut_protection)
        if region.is_empty or region.area < 0.04:
            continue
        identifier = wall.identifier + f"/{programme}/field/{key}"
        items = [
            layer(prepared, wall, identifier, region, protected, FIELD_MATERIAL, 0.035)
        ]
        if programme == "vertical-slots":
            _x0, z0, x1, z1 = rectangle.bounds
            fin = shapely.box(x1 - 0.10, z0, x1, z1).intersection(region)
            # Separate projecting fins from the dark plate with a real open joint.
            plate = region.difference(fin.buffer(JOINT_M, join_style="mitre"))
            if fin.area >= 0.04 and plate.area >= 0.04:
                items = [
                    layer(
                        prepared,
                        wall,
                        identifier + "/plate",
                        plate,
                        protected,
                        FIELD_MATERIAL,
                        0.035,
                    ),
                    layer(
                        prepared, wall, identifier + "/fin", fin, protected, FIN, 0.18
                    ),
                ]
        cells.append(
            ProgrammeCell(identifier, wall.identifier, "field", windows, tuple(items))
        )
        field_regions.append(region)
    occupied = shapely.union_all(field_regions).buffer(JOINT_M, join_style="mitre")
    mass = prepared.mass
    bounds = domain.bounds
    zones = (
        ("plinth", mass.bottom_m, mass.storeys[0].top_m, PLINTH),
        ("main", mass.storeys[0].top_m, mass.storeys[-1].bottom_m, MAIN),
        ("attic", mass.storeys[-1].bottom_m, mass.top_m, ATTIC),
    )
    for role, low, high, material in zones:
        if high <= low:
            continue
        zone = shapely.box(bounds[0], low + JOINT_M / 2, bounds[2], high - JOINT_M / 2)
        region = domain.intersection(zone).difference(cut_protection.union(occupied))
        if region.is_empty or region.area < 0.04:
            continue
        identifier = wall.identifier + f"/{programme}/{role}"
        item = layer(prepared, wall, identifier, region, protected, material, 0.012)
        cells.append(
            ProgrammeCell(
                identifier,
                wall.identifier,
                role,
                tuple(w.identifier for w in wall.windows),
                (item,),
            )
        )
    return tuple(cells)


def plan_programme(prepared, programme, *, portals=(), max_triangles=32768):
    if programme not in PROGRAMMES:
        raise ValueError("Unknown complete facade programme")
    if type(max_triangles) is not int or max_triangles < 0:
        raise ValueError("Programme capacity requires a nonnegative integer")
    windows = tuple(sorted(prepared.facade.windows, key=lambda w: w.identifier))
    if len({w.identifier for w in windows}) != len(windows):
        raise ValueError("Programme windows need unique source identities")
    prepared = replace(prepared, facade=replace(prepared.facade, windows=windows))
    portals = tuple(sorted(portals, key=lambda p: p.identifier))
    walls, rejected = domains(
        prepared, portals=portals, outward_m=0.18, include_blank=True
    )
    cells, rejected, triangles = [], list(rejected), 0
    for wall in walls:
        try:
            proposed = wall_cells(prepared, wall, programme)
            count = sum(
                layer.triangles("closed") for c in proposed for layer in c.layers
            )
            if triangles + count > max_triangles:
                raise ValueError(
                    "Complete wall programme exceeds closed reference capacity"
                )
            cells.extend(proposed)
            triangles += count
        except ValueError as error:
            rejected.append((wall.identifier, str(error)))
    return FacadeProgramme(
        prepared.mass.identifier, programme, tuple(cells), tuple(rejected)
    )
