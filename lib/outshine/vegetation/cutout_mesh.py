"""Finite grid/CDT coverage and seam-aware mapping of one prepared blade cutout."""

import numpy as np
import shapely

from ..geometry import conform_triangles
from .blade_surface import _validate_mesh


def partition(cutout, along, across):
    blade = cutout.blade
    y = np.linspace(0, 1, along + 1)
    if blade.petiole:
        y = np.r_[-blade.petiole / blade.length, y]
    x = np.linspace(-1, 1, 2 * across + 1)
    cells = shapely.box(
        np.tile(x[:-1], len(y) - 1),
        np.repeat(y[:-1], len(x) - 1),
        np.tile(x[1:], len(y) - 1),
        np.repeat(y[1:], len(x) - 1),
    )
    # A shared binary parameter precision prevents independent adjacent clips
    # from creating almost coincident vertices and zero-length seam triangles.
    precision = 2.0**-42
    pieces = shapely.intersection(cells, cutout.domain, grid_size=precision)
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(pieces))
    raw = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3].copy()
    lower = -blade.petiole / blade.length
    raw[:, :, 1] = np.where(abs(raw[:, :, 1] - lower) <= precision, lower, raw[:, :, 1])
    return raw * np.array([blade.width / 2, blade.length])


def planar_mesh(raw, cutout, max_faces, max_vertices):
    if len(raw) > max_faces:
        return None
    xy, inverse = np.unique(raw.reshape(-1, 2), axis=0, return_inverse=True)
    if len(xy) > max_vertices:
        return None
    faces = inverse.reshape(-1, 3).astype(np.int32)
    p = xy[faces]
    a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    det = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    if np.any(det == 0):
        raise ValueError("collapsed cutout parameter triangle")
    faces[det < 0] = faces[det < 0, ::-1]
    try:
        faces, _ = conform_triangles(
            xy, faces, max_faces=max_faces, max_candidates=1_000_000
        )
    except ValueError as error:
        if str(error) in (
            "interface face budget exhausted",
            "interface candidate budget exhausted",
        ):
            return None
        raise
    shapes = shapely.polygons(xy[faces])
    if not shapely.coverage_is_valid(shapes):
        raise ValueError("cutout parameter coverage is not conforming")
    difference = (
        shapely.coverage_union_all(shapes)
        .symmetric_difference(cutout.parameter_domain_m)
        .area
    )
    if difference > max(1e-18, cutout.parameter_domain_m.area * 1e-10):
        raise ValueError("cutout parameter coverage changed")
    return xy, faces.astype(np.int32)


def map_surface(cutout, xy, faces):
    blade = cutout.blade
    parameters = np.c_[xy[:, 1] / blade.length, 2 * xy[:, 0] / blade.width]
    # Clamp only rounding at exact carrier endpoints, never authored geometry.
    parameters[:, 0] = np.clip(parameters[:, 0], -blade.petiole / blade.length, 1)
    parameters[:, 1] = np.clip(parameters[:, 1], -1, 1)
    t, s = parameters.T
    vertices = blade.evaluate(t, s, np.where(s < 0, -1, 1))[0]
    corners = parameters[faces]
    side = np.where(corners[:, :, 1].mean(axis=1) < 0, -1, 1)
    petiole = corners[:, :, 0].mean(axis=1) < 0
    normals = blade.evaluate(
        corners[:, :, 0], corners[:, :, 1], side[:, None], petiole[:, None]
    )[1]
    result = {
        "vertices": vertices,
        "faces": faces,
        "normals": normals.reshape(-1, 3),
        "parameters": parameters,
    }
    _validate_mesh(result)
    return result


def cutout_mesh(cutout, along, across, max_faces, max_vertices):
    if any(
        type(v) is not int for v in (along, across, max_faces, max_vertices)
    ) or not (
        2 <= along <= 128
        and 1 <= across <= 16
        and 0 <= max_faces <= 32768
        and 0 <= max_vertices <= 32768
    ):
        raise ValueError("bounded integer cutout grid and capacities required")
    if max_faces == 0 or max_vertices == 0:
        return None
    planar = planar_mesh(
        partition(cutout, along, across), cutout, max_faces, max_vertices
    )
    if planar is None:
        return None
    return map_surface(cutout, *planar)
