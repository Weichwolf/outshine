"""Choose façade geometry by projected size while preserving the opening layout."""

from dataclasses import dataclass
from itertools import pairwise

import numpy as np
import shapely
from outshine_ref.buildings.facades import FacadeRepeat
from outshine_ref.buildings.recessed_facades import RecessPlan, recess_surface
from shapely.affinity import translate

OPENING_PIXELS = 1.5
RELIEF_M = 0.12
RELIEF_PIXELS = 1.2
BAY_PITCH_M = 3.4
STOREY_PITCH_M = 3.2
SILL_MARGIN_M = 0.075


@dataclass(frozen=True)
class FacadeView:
    eye: tuple[float, float] = (0.0, 0.0)
    focal: float = 700.0
    elevation: float = 1.7


def facade_bands(width, height, columns, grounded):
    floors = max(1, round(height / STOREY_PITCH_M))
    if grounded and floors > 1:
        lower = height / floors * 1.17
        yield 0.0, FacadeRepeat(width, lower, columns, 1, 0.49, 0.62)
        yield (
            lower,
            FacadeRepeat(width, height - lower, columns, floors - 1, 0.42, 0.55),
        )
    else:
        yield 0.0, FacadeRepeat(width, height, columns, floors, 0.42, 0.55)


def opening_axes(repeat):
    sizes = repeat.cell_size_m
    return tuple(
        (np.arange(count)[:, None] + np.array([1 - fraction, 1 + fraction]) * 0.5)
        * size
        for count, fraction, size in zip(
            (repeat.columns, repeat.rows),
            (repeat.opening_width, repeat.opening_height),
            sizes,
            strict=True,
        )
    )


def rectangles(horizontal, vertical):
    intervals = np.concatenate(
        (
            np.repeat(horizontal, len(vertical), axis=0),
            np.tile(vertical, (len(horizontal), 1)),
        ),
        axis=1,
    )
    return intervals[:, [[0, 2], [1, 2], [1, 3], [0, 3]]]


def face_grid(count):
    return (np.arange(count)[:, None, None] * 4 + [[0, 1, 2], [0, 2, 3]]).reshape(-1, 3)


def flat_bays(repeat):
    u, v = opening_axes(repeat)
    piers = np.column_stack((np.r_[0, u[:, 1]], np.r_[u[:, 0], repeat.width_m]))
    spandrels = np.column_stack((np.r_[0, v[:, 1]], np.r_[v[:, 0], repeat.height_m]))
    wall = np.concatenate(
        (rectangles(piers, v), rectangles([[0, repeat.width_m]], spandrels))
    )
    for material, quads in (("wall", wall), ("glass", rectangles(u, v))):
        xy = quads.reshape(-1, 2)
        yield material, np.column_stack((xy, np.zeros(len(xy)))), face_grid(len(quads))


def members(u, v, setback, projection):
    corners = rectangles(u, v)
    planes = (
        np.broadcast_to([setback, projection], (len(corners), 4, 2))
        .transpose(0, 2, 1)
        .reshape(-1, 8, 1)
    )
    points = np.concatenate((np.tile(corners, (1, 2, 1)), planes), axis=2)
    quads = [
        [3, 2, 1, 0],
        [4, 5, 6, 7],
        [0, 1, 5, 4],
        [1, 2, 6, 5],
        [2, 3, 7, 6],
        [3, 0, 4, 7],
    ]
    signs = np.asarray(quads)[:, [0, 1, 2, 0, 2, 3]].reshape(-1, 3)
    faces = np.arange(len(points))[:, None, None] * 8 + signs
    return points.reshape(-1, 3), faces.reshape(-1, 3)


def detailed_bays(repeat):
    plan = RecessPlan(repeat)
    surface = recess_surface(plan)
    for role, name in enumerate(("wall", "glass", "frame")):
        faces = surface["faces"][surface["roles"] == role]
        used, inverse = np.unique(faces, return_inverse=True)
        yield name, surface["vertices"][used], inverse.reshape(-1, 3)
    u, v = opening_axes(repeat)
    ledge = v[:, :1] + [-SILL_MARGIN_M, 0.0]
    yield (
        "window sill",
        *members(u + [-SILL_MARGIN_M, SILL_MARGIN_M], ledge, -0.025, RELIEF_M),
    )
    if (u[0, 1] - u[0, 0]) > 1.05:
        sash = u.mean(axis=1)[:, None] + [-0.0275, 0.0275]
        yield (
            "frame",
            *members(
                sash, v + [plan.frame_m[1], -plan.frame_m[1]], -plan.depth_m, -0.07
            ),
        )


def wall_panel(meshes, a, b, bottom, top, material):
    meshes.add(
        material,
        np.column_stack(([a, b, b, a], [bottom, bottom, top, top])),
        ((0, 1, 2), (0, 2, 3)),
    )


def placed(meshes, material, vertices, faces, a, axis, normal, bottom):
    u, v, n = vertices.T
    xy = a + u[:, None] * axis + n[:, None] * normal
    meshes.add(material, np.column_stack((xy, bottom + v)), faces)


def clipped_wall(visible, holes):
    area = visible.difference(shapely.union_all(holes)) if holes else visible
    triangles = shapely.orient_polygons(
        shapely.get_parts(shapely.constrained_delaunay_triangles(area))
    )
    points = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3].reshape(-1, 2)
    return np.column_stack((points, np.zeros(len(points)))), np.arange(
        len(points)
    ).reshape(-1, 3)


def intact_windows(visible, repeat, start, rise, detailed):
    u, v = opening_axes(repeat)
    quads = rectangles(u + start, v + rise)
    x0, z0 = quads[:, 0].T
    x1, z1 = quads[:, 2].T
    margin = SILL_MARGIN_M if detailed else 0.0
    clearance = shapely.box(x0 - margin, z0 - margin, x1 + margin, z1)
    intact = shapely.covers(visible, clearance)
    holes = shapely.box(x0[intact], z0[intact], x1[intact], z1[intact])
    return intact, quads[intact], holes


def isolated_bays(repeat, intact, quads, start, rise, *, detailed):
    if not len(quads):
        return
    if not detailed:
        vertices = quads.reshape(-1, 2)
        yield (
            "glass",
            np.column_stack((vertices, np.zeros(len(vertices)))),
            face_grid(len(quads)),
        )
        return
    grid = np.indices((repeat.columns, repeat.rows)).reshape(2, -1).T
    offsets = grid[intact] * repeat.cell_size_m + [start, rise]
    offsets = np.column_stack((offsets, np.zeros(len(offsets))))
    cell = FacadeRepeat(
        *repeat.cell_size_m, 1, 1, repeat.opening_width, repeat.opening_height
    )
    for name, points, faces in detailed_bays(cell):
        if name == "wall":
            faces = faces[np.any(points[faces, 2] != 0.0, axis=1)]
        vertices = points[None, :, :] + offsets[:, None, :]
        indices = faces[None, :, :] + np.arange(len(offsets))[:, None, None] * len(
            points
        )
        yield name, vertices.reshape(-1, 3), indices.reshape(-1, 3)


def bay_detail(view, offset, axis, normal, pitch, columns, bottom, top):
    steps = np.arange(columns) * pitch
    along = offset @ axis
    cross = abs(offset @ normal)
    horizontal = along - np.clip(along, steps, steps + pitch)
    rise = view.elevation - np.clip(view.elevation, bottom, top)
    distance = np.maximum(1.0, np.sqrt(horizontal**2 + cross**2 + rise**2))
    scale = view.focal / distance
    facing = np.minimum(1.0, cross / distance)
    storey = (top - bottom) / max(1, round((top - bottom) / STOREY_PITCH_M))
    visible = scale * np.minimum(pitch * 0.42 * facing, storey * 0.55) >= OPENING_PIXELS
    detailed = scale * RELIEF_M >= RELIEF_PIXELS
    return np.where(visible, np.where(detailed, 2, 1), 0)


def draw_facade(
    meshes, a, b, bottom, top, view, *, grounded, bays, wall="wall", exposed=None
):
    span = b - a
    width = np.linalg.norm(span)
    if width < 0.01:
        return
    if exposed is not None and exposed.is_empty:
        return
    axis = span / width
    normal = np.array([axis[1], -axis[0]])
    height = top - bottom
    visible = translate(exposed, yoff=-bottom) if exposed is not None else None
    if not bays or width <= 2 or height <= 2:
        if visible is None:
            wall_panel(meshes, a, b, bottom, top, wall)
        else:
            vertices, faces = clipped_wall(visible, [])
            placed(meshes, wall, vertices, faces, a, axis, normal, bottom)
        return
    offset = np.asarray(view.eye) - a
    columns = max(1, round(width / BAY_PITCH_M))
    pitch = width / columns
    detail = bay_detail(view, offset, axis, normal, pitch, columns, bottom, top)
    boundaries = np.r_[0, np.flatnonzero(detail[1:] != detail[:-1]) + 1, columns]
    holes = []
    for start, stop in pairwise(boundaries):
        offset_m = start * pitch
        origin = a + offset_m * axis
        span_m = (stop - start) * pitch
        if detail[start] == 0:
            if visible is None:
                wall_panel(meshes, origin, a + stop * pitch * axis, bottom, top, wall)
            continue
        detailed = detail[start] == 2
        emit = detailed_bays if detailed else flat_bays
        for rise, repeat in facade_bands(span_m, height, int(stop - start), grounded):
            if visible is None:
                for name, points, faces in emit(repeat):
                    placed(
                        meshes,
                        wall if name == "wall" else name,
                        points,
                        faces,
                        origin,
                        axis,
                        normal,
                        bottom + rise,
                    )
            else:
                intact, quads, openings = intact_windows(
                    visible, repeat, offset_m, rise, detailed
                )
                holes.extend(openings)
                for name, points, faces in isolated_bays(
                    repeat, intact, quads, offset_m, rise, detailed=detailed
                ):
                    placed(
                        meshes,
                        wall if name == "wall" else name,
                        points,
                        faces,
                        a,
                        axis,
                        normal,
                        bottom,
                    )
    if visible is not None:
        vertices, faces = clipped_wall(visible, holes)
        placed(meshes, wall, vertices, faces, a, axis, normal, bottom)
