"""Draw the street storey, recessed entries and low elevations."""

from itertools import pairwise

import numpy as np
import shapely
from entrances import (
    APPROACH_DEPTH_M,
    APPROACH_WIDTH_M,
    DOOR_HEIGHT_M,
    DOOR_WIDTH_M,
    TREAD_DEPTH_M,
    edge_frame,
)
from facade import (
    BAY_PITCH_M,
    RELIEF_PIXELS,
    bay_detail,
    clipped_wall,
    face_grid,
    members,
    placed,
    rectangles,
)
from shapely.affinity import translate


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
    rise = bottom - front.ground
    count = int(np.ceil(rise / 0.165))
    depth = np.r_[0.0, APPROACH_DEPTH_M - np.arange(count - 1, -1, -1) * TREAD_DEPTH_M]
    for i, (begin, end) in enumerate(pairwise(depth)):
        height = -i * rise / count
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


def draw_low_front(meshes, a, b, bottom, top, wall, view, front):
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


def street_openings(width, height, detail, entry):
    pitch = width / len(detail)
    stations = (np.arange(len(detail)) + 0.5) * pitch
    door = np.argmin(abs(stations - entry.station)) if entry else -1
    half_window = min(0.66, pitch * 0.27)
    for i, (u, lod) in enumerate(zip(stations, detail, strict=True)):
        if i == door:
            x = entry.station
            yield (
                (x - DOOR_WIDTH_M / 2, 0.0, x + DOOR_WIDTH_M / 2, DOOR_HEIGHT_M),
                "entrance door",
                lod == 2,
            )
        elif lod:
            yield (
                (u - half_window, 0.91, u + half_window, height - 0.72),
                "glass",
                lod == 2,
            )


def draw_street_storey(meshes, a, b, bottom, top, wall, view, exposed, entry):
    if np.linalg.norm(b - a) < 0.01:
        return
    width, axis, normal = edge_frame(a, b)
    height = top - bottom
    face = (
        shapely.box(0, 0, width, height)
        if exposed is None
        else translate(exposed, yoff=-bottom)
    )
    if face.is_empty:
        return
    columns = max(1, round(width / BAY_PITCH_M))
    detail = bay_detail(
        view,
        np.asarray(view.eye) - a,
        axis,
        normal,
        width / columns,
        columns,
        bottom,
        top,
    )
    if width <= 2.0:
        detail.fill(0)
    holes = []
    for opening, material, relief in street_openings(width, height, detail, entry):
        hole = shapely.box(*opening)
        if not face.covers(hole):
            continue
        holes.append(hole)
        for name, vertices, faces in opening_parts(opening, material, relief):
            placed(meshes, name, vertices, faces, a, axis, normal, bottom)
        if material == "entrance door":
            for name, vertices, faces in entrance_steps(entry, bottom):
                placed(meshes, name, vertices, faces, a, axis, normal, bottom)
    base = wall.replace("wall", "street base", 1)
    placed(meshes, base, *clipped_wall(face, holes), a, axis, normal, bottom)
    if (detail == 2).any():
        belt = shapely.LineString([(0, height - 0.13), (width, height - 0.13)])
        spans = [
            [part.bounds[0], part.bounds[2]]
            for part in shapely.get_parts(face.intersection(belt))
            if part.length > 0.01
        ]
        if spans:
            trim = members(spans, [[height - 0.20, height - 0.06]], 0.001, 0.085)
            placed(meshes, "street trim", *trim, a, axis, normal, bottom)
