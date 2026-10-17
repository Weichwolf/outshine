"""Direct polygonal exterior with flat window regions; no floors, stairs or 3D Boolean."""

from dataclasses import dataclass
from itertools import pairwise
from types import MappingProxyType

import numpy as np
import shapely
from building_layout import rectangle
from building_mesh import solid
from building_plan import FLOOR_M
from outshine_ref.buildings.modelling.faces import Face, compile_faces
from outshine_ref.buildings.modelling.prism_union import frozen
from outshine_ref.buildings.rooflight_panels import triangle_surface
from terraced_facade_flat import windows as flat_windows
from terraced_facade_prepare import positions


@dataclass(frozen=True)
class CompactFacade:
    mesh: object
    owner_ids: tuple
    triangle_owners: object
    window_ids: tuple


def polygon_parts(geometry):
    if geometry.is_empty:
        return
    if geometry.geom_type == "Polygon":
        yield geometry
    elif geometry.geom_type in ("MultiPolygon", "GeometryCollection"):
        for part in shapely.get_parts(geometry):
            yield from polygon_parts(part)
    elif geometry.area != 0:
        raise ValueError("Unexpected nonpolygonal compact surface region")


class Surfaces:
    def __init__(self):
        self.vertices, self.faces, self.owners = [], [], []
        self.quads, self.quad_materials, self.quad_owners = [], [], []

    def polygon(self, polygon, origin, u, v, material, owner, *, reverse=False):
        for part in polygon_parts(polygon):
            if part.is_empty:
                continue
            if part.geom_type != "Polygon" or not part.is_valid:
                raise ValueError("Compact exterior requires valid polygonal regions")
            rings = []
            for ring in (part.exterior, *part.interiors):
                xy = np.asarray(ring.coords)[:-1]
                if reverse:
                    xy = xy[::-1]
                start = len(self.vertices)
                self.vertices.extend(
                    np.asarray(origin) + xy[:, :1] * u + xy[:, 1:2] * v
                )
                rings.append(tuple(range(start, len(self.vertices))))
            self.faces.append(Face(rings[0], tuple(rings[1:]), material))
            self.owners.append(owner)

    def segment(self, line, low, high, material, owner):
        for part in shapely.get_parts(line):
            if part.geom_type != "LineString" or part.length <= 1e-10:
                continue
            a, b = np.asarray(part.coords)[[0, -1]]
            self.quads.append(
                [np.r_[a, low], np.r_[b, low], np.r_[b, high], np.r_[a, high]]
            )
            self.quad_materials.append(material)
            self.quad_owners.append(owner)


def wall(writer, mass, windows, storey_index, ring_index, edge_index, a, b):
    storey = mass.storeys[storey_index]
    length = float(np.linalg.norm(b - a))
    tangent = (b - a) / length
    outward = np.array([tangent[1], -tangent[0]])
    holes = []
    for w in windows:
        centre = float((np.asarray(w.centre) - a) @ tangent)
        hole = shapely.box(
            centre - w.width_m / 2,
            w.bottom_m - storey.bottom_m,
            centre + w.width_m / 2,
            w.bottom_m + w.height_m - storey.bottom_m,
        )
        holes.append(tuple(hole.exterior.coords)[:-1])
    if storey_index == 0:
        centre = np.asarray(mass.entry.centre)
        along = float((centre - a) @ tangent)
        if abs(float((centre - a) @ outward)) < 1e-7 and 0.55 < along < length - 0.55:
            door = shapely.box(
                along - 0.55, FLOOR_M, along + 0.55, FLOOR_M + mass.entry.height_m
            )
            holes.append(tuple(door.exterior.coords)[:-1])
    polygon = shapely.Polygon(
        [
            (0, 0),
            (length, 0),
            (length, storey.top_m - storey.bottom_m),
            (0, storey.top_m - storey.bottom_m),
        ],
        holes,
    )
    writer.polygon(
        polygon,
        np.r_[a, storey.bottom_m],
        np.r_[tangent, 0],
        np.array([0, 0, 1]),
        12,
        storey.identifier + f"/wall/{ring_index}/{edge_index}",
    )


def accents(writer, mass, facade):
    keepout = rectangle(mass.entry.centre, mass.entry.tangent, 1.30, 0.35, 0.60)
    for i, storey in enumerate(mass.storeys):
        if mass.kind != "cascade":
            continue
        polygon = shapely.orient_polygons(shapely.from_wkb(storey.footprint_wkb))
        for ring in (polygon.exterior, *polygon.interiors):
            for a, b in pairwise(np.asarray(ring.coords)):
                tangent = (b - a) / np.linalg.norm(b - a)
                outward = np.array([tangent[1], -tangent[0]])
                line = shapely.LineString([a + 0.10 * outward, b + 0.10 * outward])
                if i == 0:
                    line = line.difference(keepout)
                writer.segment(
                    line,
                    storey.bottom_m + 0.025,
                    storey.bottom_m + 0.20,
                    9,
                    storey.identifier + "/band",
                )
    if mass.kind == "paired-wings":
        for w in facade.windows:
            tangent = np.asarray(w.tangent)
            outward = np.array([tangent[1], -tangent[0]])
            centre = (
                np.asarray(w.centre) + tangent * (w.width_m / 2 + 0.22) + outward * 0.14
            )
            line = shapely.LineString(
                [centre - 0.08 * tangent, centre + 0.08 * tangent]
            )
            if w.storey == 0:
                line = line.difference(keepout)
            s = mass.storeys[w.storey]
            writer.segment(line, s.bottom_m + 0.06, s.top_m - 0.06, 9, w.identifier)


def caps(prepared):
    mass = prepared.mass
    points, owners = [], []
    regions = [
        (
            shapely.from_wkb(mass.source_wkb),
            mass.bottom_m,
            mass.identifier + "/underside",
            False,
        )
    ]
    for index, s in enumerate(mass.storeys):
        wkb = (
            s.footprint_wkb
            if index == len(mass.storeys) - 1
            else mass.storeys[index + 1].terrace_wkb
        )
        regions.append((shapely.from_wkb(wkb), s.top_m, s.identifier + "/roof", True))
    for region, height, owner, upward in regions:
        for part in polygon_parts(region):
            if not part.is_valid:
                raise ValueError("Original compact cap must remain valid")
            for triangle in shapely.get_parts(
                shapely.constrained_delaunay_triangles(part)
            ):
                xy = np.asarray(triangle.exterior.coords)[:3]
                cross = np.cross(np.r_[xy[1] - xy[0], 0], np.r_[xy[2] - xy[0], 0])[2]
                if (cross > 0) != upward:
                    xy = xy[::-1]
                points.extend(np.c_[xy, np.full(3, height)])
                owners.append(owner)
    points = np.asarray(points)
    return triangle_surface(
        {"vertices": points, "faces": np.arange(len(points)).reshape(-1, 3)}, 12
    ), owners


def joined(meshes):
    offsets = np.cumsum([0] + [len(m["vertices"]) for m in meshes[:-1]])
    if sum(len(m["vertices"]) for m in meshes) > np.iinfo(np.uint32).max:
        raise ValueError("Compact exterior exceeds its32-bit vertex index capacity")
    return {
        key: np.concatenate(
            [
                m[key] + int(offset) if key == "faces" else m[key]
                for m, offset in zip(meshes, offsets, strict=True)
            ]
        )
        for key in meshes[0]
    }


def compact(prepared):
    mass, facade = prepared.local, prepared.facade
    writer = Surfaces()
    by_edge = {}
    for window in facade.windows:
        by_edge.setdefault((window.storey, *window.edge), []).append(window)
    for index, storey in enumerate(mass.storeys):
        polygon = shapely.orient_polygons(shapely.from_wkb(storey.footprint_wkb))
        for ri, ring in enumerate((polygon.exterior, *polygon.interiors)):
            for ei, (a, b) in enumerate(pairwise(np.asarray(ring.coords))):
                if np.linalg.norm(b - a) > 1e-10:
                    wall(
                        writer,
                        mass,
                        by_edge.get((index, ri, ei), ()),
                        index,
                        ri,
                        ei,
                        a,
                        b,
                    )
    accents(writer, mass, facade)
    pad = shapely.from_wkb(mass.entry.landing_wkb).difference(
        shapely.from_wkb(mass.source_wkb)
    )
    storage = [], [], [], []
    solid(storage, pad, mass.bottom_m, mass.bottom_m + FLOOR_M, 4)
    start = len(writer.vertices)
    writer.vertices.extend(storage[0])
    for triangle in storage[1]:
        writer.faces.append(Face(tuple(start + i for i in triangle), material=4))
        writer.owners.append(mass.entry.identifier + "/pad")
    raw, _topology = compile_faces(np.asarray(writer.vertices), writer.faces)
    mesh = triangle_surface(raw, 12)
    mesh["materials"] = raw["materials"]
    quad_points = np.asarray(writer.quads).reshape(-1, 3)
    quad_faces = (
        np.arange(len(quad_points))
        .reshape(-1, 4)[:, [[0, 1, 2], [0, 2, 3]]]
        .reshape(-1, 3)
    )
    qmesh = triangle_surface({"vertices": quad_points, "faces": quad_faces}, 9)
    qmesh["materials"] = np.repeat(writer.quad_materials, 2).astype(np.uint16)
    wmesh, window_owners = flat_windows(facade)
    mesh = joined((mesh, qmesh, wmesh))
    mesh["vertices"] = positions(mesh["vertices"], prepared.mass.core)
    normals = mesh["normals"].copy()
    normals[:, :2] = normals[:, :1] * np.asarray(
        prepared.mass.core.across_xy
    ) + normals[:, 1:2] * np.asarray(prepared.mass.core.along_xy)
    mesh["normals"] = normals
    horizontal, names = caps(prepared)
    mesh = joined((mesh, horizontal))
    window_base = len(writer.owners) + len(writer.quad_owners)
    names_base = window_base + len(facade.windows)
    owner_indices = np.r_[
        raw["parts"],
        np.repeat(np.arange(len(writer.quad_owners)) + len(writer.owners), 2),
        window_owners + window_base,
        np.arange(len(names)) + names_base,
    ].astype(np.uint32)
    return CompactFacade(
        MappingProxyType({k: frozen(v) for k, v in mesh.items()}),
        tuple(writer.owners)
        + tuple(writer.quad_owners)
        + tuple(w.identifier for w in facade.windows)
        + tuple(names),
        frozen(owner_indices),
        tuple(w.identifier for w in facade.windows),
    )
