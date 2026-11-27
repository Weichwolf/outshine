"""Front doors and low elevations for isolated buildings reached from paths."""

from dataclasses import dataclass
from itertools import pairwise

import numpy as np
import shapely
from facade import RELIEF_PIXELS, clipped_wall, face_grid, members, placed, rectangles
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


@dataclass(frozen=True)
class Frontage:
    side: int
    station: float
    ground: float
    approach: shapely.Polygon


def edge_frame(a, b):
    span = b - a
    length = np.linalg.norm(span)
    axis = span / length
    return length, axis, axis[::-1] * [1, -1]


def at_path(polygon, paths, obstacles, reserved):
    candidates = []
    for side, (a, b) in enumerate(pairwise(np.asarray(polygon.exterior.coords))):
        if np.linalg.norm(b - a) < 3.2:
            continue
        length, axis, normal = edge_frame(a, b)
        for fraction in (0.5, 0.25, 0.75):
            u = np.clip(fraction * length, 1.4, length - 1.4)
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
        return min(candidates, key=lambda row: row[:3])[1:]


def frontages(forms, neighbours, features, water, ground):
    eligible = [
        (index, form)
        for index, (form, nearby) in enumerate(zip(forms, neighbours, strict=True))
        if form.foundation
        and form.foundation.apron_m > 0
        and not nearby
        and not form.passages
        and 3.15 <= form.top - form.bottom <= 5.4
        and form.polygon.area >= 180
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
    for index, form in eligible:
        nearby = footprints.query(form.polygon, "dwithin", 20.0)
        barriers = shapely.STRtree(footprints.geometries[nearby[nearby != index]])
        match = at_path(form.polygon, paths, barriers, reserved)
        if match is not None:
            matches.append((index, *match))
    if not matches:
        return {}
    samples = np.array(
        [
            point + normal * APPROACH_DEPTH_M + lateral * axis
            for _, _, _, point, axis, normal, _ in matches
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
    return {
        i: Frontage(side, station, float(heights.max()), approach)
        for (i, side, station, _, _, _, approach), heights in zip(
            matches, z, strict=True
        )
        if np.isfinite(heights).all()
        and np.ptp(heights) < 0.18
        and 0.12 < forms[i].bottom - heights.max() <= 0.61
    }


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


def front_windows(width, height, door):
    for low, high in ((0.85, door - 1.35), (door + 1.35, width - 0.85)):
        span = high - low
        if span < 1.08:
            continue
        count = max(1, int(span / 2.15))
        pitch = span / count
        for x in low + (np.arange(count) + 0.5) * pitch:
            half = min(0.92, pitch * 0.38)
            yield x - half, 1.05, x + half, min(2.20, height - 0.65)


def opening_parts(opening, material, detail):
    x0, z0, x1, z1 = opening
    depth = 0.135 if detail else 0.0
    profile = rectangles([[x0, x1]], [[z0, z1]])[0]
    panel = np.column_stack((profile, np.full(4, -depth)))
    yield material, panel, face_grid(1)
    if detail:
        reveal = np.vstack((panel, np.column_stack((profile, np.zeros(4)))))
        sides = np.array([[0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]])
        yield "window sill", reveal, sides[:, [0, 1, 2, 0, 2, 3]].reshape(-1, 3)
        vertical = [[x0, x0 + 0.055], [x1 - 0.055, x1]]
        if material == "glass" and x1 - x0 >= 1.25:
            midpoint = (x0 + x1) / 2
            vertical.append([midpoint - 0.023, midpoint + 0.023])
        yield "frame", *members(vertical, [[z0, z1]], -depth, -0.045)
        yield "frame", *members([[x0, x1]], [[z1 - 0.055, z1]], -depth, -0.045)


def entrance_steps(front, bottom):
    u = np.array(
        [front.station - APPROACH_WIDTH_M / 2, front.station + APPROACH_WIDTH_M / 2]
    )
    depth = np.r_[0.0, LANDING_DEPTH_M + np.arange(4) * TREAD_DEPTH_M]
    rise = bottom - front.ground
    for i, (begin, end) in enumerate(pairwise(depth)):
        height = -i * rise / 4
        points = [
            [u[0], height, begin],
            [u[1], height, begin],
            [u[1], height, end],
            [u[0], height, end],
        ]
        skirt = [[x, -rise - 0.16, n] for x, _, n in points]
        faces = [
            [0, 2, 1],
            [0, 3, 2],
            [0, 4, 7],
            [0, 7, 3],
            [1, 2, 6],
            [1, 6, 5],
            [3, 7, 6],
            [3, 6, 2],
        ]
        yield "foundation", np.asarray([*points, *skirt]), np.asarray(faces)


def draw_front(meshes, a, b, bottom, top, wall, view, front):
    width, axis, normal = edge_frame(a, b)
    height = top - bottom
    distance = np.linalg.norm(a + axis * front.station - np.asarray(view.eye))
    detail = view.focal * 0.135 >= RELIEF_PIXELS * max(1.0, distance)
    half = DOOR_WIDTH_M / 2
    door = (front.station - half, 0, front.station + half, DOOR_HEIGHT_M)
    windows = list(front_windows(width, height, front.station))
    for opening, material in [
        (door, "entrance door"),
        *((x, "glass") for x in windows),
    ]:
        for name, vertices, faces in opening_parts(opening, material, detail):
            placed(meshes, name, vertices, faces, a, axis, normal, bottom)
    holes = list(shapely.box(*np.array([door, *windows]).T))
    wall_parts = clipped_wall(shapely.box(0, 0, width, height), holes)
    placed(meshes, wall, *wall_parts, a, axis, normal, bottom)
    for name, vertices, faces in entrance_steps(front, bottom):
        placed(meshes, name, vertices, faces, a, axis, normal, bottom)
