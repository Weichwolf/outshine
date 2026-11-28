"""Choose accessible entrances and fit their connections to solved terrain."""

from dataclasses import dataclass
from itertools import pairwise

import numpy as np
import shapely
from building_envelope import exposed_wall
from facade import BAY_PITCH_M
from ground_surface import (
    ROUTE_MATERIALS,
    SOIL_MATERIALS,
    at_grade_width,
    partition_triangles,
    route_area,
    surface_elevations,
)

DOOR_WIDTH_M = 1.35
DOOR_HEIGHT_M = 2.24
APPROACH_WIDTH_M = 2.05
LANDING_DEPTH_M = 0.72
TREAD_DEPTH_M = 0.30
APPROACH_DEPTH_M = LANDING_DEPTH_M + 3 * TREAD_DEPTH_M
SHARED_ENTRY_SPACING_M = 21.0
STREET_STOREY_M = 3.35


@dataclass(frozen=True)
class Frontage:
    side: int
    station: float
    ground: float
    approach: shapely.Polygon
    low: bool


def edge_frame(a, b):
    span = b - a
    length = np.linalg.norm(span)
    axis = span / length
    return length, axis, axis[::-1] * [1, -1]


def has_street_storey(form):
    height = form.top - form.bottom
    return bool(
        form.foundation
        and (height >= 6.4 or (form.group is not None and height >= 4.4))
    )


def at_path(form, paths, obstacles, reserved, neighbours, tall):
    polygon = form.polygon
    candidates = []
    for side, (a, b) in enumerate(pairwise(np.asarray(polygon.exterior.coords))):
        if np.linalg.norm(b - a) < 3.2:
            continue
        length, axis, normal = edge_frame(a, b)
        if tall:
            pitch = length / max(1, round(length / BAY_PITCH_M))
            stations = np.arange(pitch / 2, length, pitch)
            visible = exposed_wall(
                a, b, form.bottom, form.bottom + STREET_STOREY_M, neighbours
            )
        else:
            stations = np.clip(np.array([0.5, 0.25, 0.75]) * length, 1.4, length - 1.4)
            visible = None
        for u in stations:
            if visible is not None and not visible.covers(
                shapely.box(
                    u - 0.78, form.bottom, u + 0.78, form.bottom + DOOR_HEIGHT_M + 0.15
                )
            ):
                continue
            point = a + axis * u
            centre = shapely.Point(point)
            nearby = paths.query(centre, "dwithin", 18.0)
            if not len(nearby):
                continue
            across = axis * (APPROACH_WIDTH_M / 2)
            start = point + 0.02 * normal
            end = point + APPROACH_DEPTH_M * normal
            apron = shapely.Polygon(
                [start - across, start + across, end + across, end - across]
            )
            if (
                any(part.intersects(apron) for part in (polygon, reserved))
                or obstacles.query(apron, "intersects").size
            ):
                continue
            if paths.query(apron, "dwithin", 0.9).size:
                continue
            for index in nearby:
                route = paths.geometries[index]
                connector = shapely.shortest_line(centre, route)
                delta = shapely.get_coordinates(connector)[-1] - point
                distance = np.linalg.norm(delta)
                if distance < 2.35 or delta @ normal < 0.55 * distance:
                    continue
                access = shapely.LineString([end - 0.35 * normal, point + delta])
                approach = apron.union(route_area(access, 1.55))
                if polygon.intersects(approach) or reserved.intersects(approach):
                    continue
                if obstacles.query(approach, "intersects").size:
                    continue
                score = distance + abs(u - length / 2) * 0.20
                candidates.append((score, side, u, point, axis, normal, approach))
    if candidates:
        return min(candidates, key=lambda row: row[:3])


def frontages(forms, neighbours, features, water, ground, wall_ground=None):
    eligible = [
        (index, form, tall)
        for index, (form, nearby) in enumerate(zip(forms, neighbours, strict=True))
        for tall in (has_street_storey(form),)
        if form.foundation
        and not form.passages
        and (
            (tall and form.polygon.area >= 90)
            or (
                not tall
                and not nearby
                and form.foundation.apron_m > 0
                and 3.15 <= form.top - form.bottom <= 5.4
                and form.polygon.area >= 180
            )
        )
    ]
    if not eligible:
        return {}
    transport = [
        (shape, tags) for layer, shape, tags in features if layer == "transportation"
    ]
    paths = shapely.STRtree(
        [
            shape
            for shape, tags in transport
            if tags.get("class") == "path"
            and tags.get("subclass") not in {"steps", "bridleway"}
            and tags.get("foot") not in {"no", "private"}
            and at_grade_width(tags)
        ]
    )
    reserved = shapely.union_all(
        [
            water,
            *(
                route_area(shape, width)
                for shape, tags in transport
                if tags.get("class") != "path" and (width := at_grade_width(tags))
            ),
        ]
    )
    footprints = shapely.STRtree([form.polygon for form in forms])
    matches = []
    for index, form, tall in eligible:
        nearby = footprints.query(form.polygon, "dwithin", 20.0)
        barriers = shapely.STRtree(footprints.geometries[nearby[nearby != index]])
        match = at_path(form, paths, barriers, reserved, neighbours[index], tall)
        if match is not None:
            matches.append((index, tall, *match))
    if not matches:
        return {}
    samples = np.array(
        [
            point + normal * APPROACH_DEPTH_M + lateral * axis
            for _, _, _, _, _, point, axis, normal, _ in matches
            for lateral in (-APPROACH_WIDTH_M / 2, 0.0, APPROACH_WIDTH_M / 2)
        ]
    )
    supports = [
        surface_elevations(ground[name], samples)
        for name in (*SOIL_MATERIALS, *ROUTE_MATERIALS)
        if name in ground
    ]
    if not supports:
        return {}
    z = np.max(supports, axis=0).reshape(-1, 3)
    grouped = {}
    result = {}
    for match, heights in sorted(
        zip(matches, z, strict=True), key=lambda row: row[0][2]
    ):
        i, tall, _, side, station, point, _, normal, approach = match
        if not np.isfinite(heights).all() or np.ptp(heights) >= 0.18:
            continue
        ground_z = float(heights.max())
        min_rise = 0.015 if tall else 0.12
        if not min_rise < forms[i].bottom - ground_z <= 0.61:
            continue
        if wall_ground is not None:
            bottom = forms[i].bottom
            a, b = np.asarray(forms[i].polygon.exterior.coords)[side : side + 2]
            below = wall_ground.buried(a, b, bottom, bottom + DOOR_HEIGHT_M)
            half = DOOR_WIDTH_M / 2
            opening = shapely.box(
                station - half, bottom, station + half, bottom + DOOR_HEIGHT_M
            )
            if below is not None and below.intersects(opening):
                continue
        group = forms[i].group
        if group is not None:
            siblings = grouped.setdefault(group, [])
            if any(
                np.linalg.norm(point - old_point) < SHARED_ENTRY_SPACING_M
                and normal @ old_normal > 0.5
                for old_point, old_normal in siblings
            ):
                continue
            siblings.append((point, normal))
        result[i] = Frontage(side, station, ground_z, approach, low=not tall)
    return result


def pave_entrances(meshes, fronts):
    if not fronts:
        return
    paving = shapely.union_all([front.approach for front in fronts.values()])
    for material in SOIL_MATERIALS:
        if material not in meshes:
            continue
        vertices, triangles = map(np.asarray, meshes.pop(material))
        if not len(triangles):
            continue
        for name, points, faces in partition_triangles(
            vertices,
            triangles,
            np.zeros(len(triangles), dtype=int),
            {0: material, 1: "paving"},
            {1: paving},
        ):
            if len(faces):
                used, inverse = np.unique(faces, return_inverse=True)
                meshes.add(name, points[used], inverse.reshape(-1, 3))
