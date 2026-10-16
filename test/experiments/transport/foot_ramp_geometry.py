"""Finite flared ramp caps and a closed zero-height boundary without buried thickness."""

from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.transport.supports import SupportBudget, SupportVolumeBuilder

HEIGHT_M = 0.12
LENGTH_M = 1.9
LANDING_END_M = 2.1
FLARE_M = 2.4
RETURN_M = 2.4
FRONT_MARGIN_M = 0.01


@dataclass(frozen=True)
class Ramp:
    body: Mesh
    lower: Mesh
    upper: Mesh
    shape: object


def zones(half, landing_end=LANDING_END_M):
    if not np.isfinite(half) or not 0.3 <= half <= 2:
        raise ValueError("bounded explicit ramp half-width required")
    if not np.isfinite(landing_end) or not LENGTH_M <= landing_end <= 4.5:
        raise ValueError("bounded actual outer ramp landing required")
    width = half + FLARE_M
    return [
        shapely.Polygon([[-half, 0], [half, 0], [width, LENGTH_M], [-width, LENGTH_M]]),
        shapely.Polygon(
            [
                [-width, -FRONT_MARGIN_M],
                [-half, -FRONT_MARGIN_M],
                [-half, 0],
                [-width, LENGTH_M],
            ]
        ),
        shapely.Polygon(
            [
                [half, -FRONT_MARGIN_M],
                [width, -FRONT_MARGIN_M],
                [width, LENGTH_M],
                [half, 0],
            ]
        ),
        shapely.box(-width, LENGTH_M, width, landing_end),
        shapely.Polygon(
            [
                [-width, landing_end],
                [width, landing_end],
                [half, landing_end + RETURN_M],
                [-half, landing_end + RETURN_M],
            ]
        ),
    ]


def relative_height(local, half, landing_end=LANDING_END_M):
    s, t = local.T
    front = np.maximum(
        HEIGHT_M * t / LENGTH_M, HEIGHT_M * np.maximum(abs(s) - half, 0) / FLARE_M
    )
    back = HEIGHT_M * (landing_end + RETURN_M - t) / RETURN_M
    h = np.where(t < LENGTH_M, front, np.where(t <= landing_end, HEIGHT_M, back))
    h[abs(h) <= 1e-10] = 0
    return h


def outline(origin, basis, half, landing_end=LANDING_END_M):
    local = shapely.union_all(zones(half, landing_end))
    xy = np.asarray(local.exterior.coords) @ basis.T + origin
    return shapely.Polygon(xy)


def weld(vertices, triangles, method):
    if method == "vector":
        points, inverse = np.unique(vertices, axis=0, return_inverse=True)
    elif method == "scalar":
        keys = sorted(set(map(tuple, vertices)))
        lookup = {point: i for i, point in enumerate(keys)}
        points = np.array(keys)
        inverse = np.array([lookup[tuple(p)] for p in vertices])
    else:
        raise ValueError("known equivalent ramp closure method required")
    faces = inverse[triangles]
    valid = np.all(np.diff(np.sort(faces, axis=1), axis=1) != 0, axis=1)
    return Mesh(points, faces[valid])


def close_caps(upper, lower, *, method="vector"):
    if (
        not np.array_equal(upper.vertices[:, :2], lower.vertices[:, :2])
        or not np.array_equal(upper.triangles, lower.triangles)
        or np.any(upper.vertices[:, 2] < lower.vertices[:, 2])
    ):
        raise ValueError("ordered caps must share exact XY and indexed triangles")
    faces = upper.triangles
    edges = np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]])
    _keys, inverse, counts = np.unique(
        np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    boundary = edges[counts[inverse] == 1]
    n = len(upper.vertices)
    a, b = boundary.T
    walls = np.vstack([np.c_[a, b + n, b], np.c_[a, a + n, b + n]])
    shell = np.vstack([faces, faces[:, ::-1] + n, walls])
    return weld(np.vstack([upper.vertices, lower.vertices]), shell, method)


def build(
    origin,
    basis,
    triangle,
    half,
    *,
    landing_end=LANDING_END_M,
    footprint=None,
    method="vector",
):
    origin, basis, triangle = (
        np.asarray(origin, float),
        np.asarray(basis, float),
        np.asarray(triangle, float),
    )
    if (
        origin.shape != (2,)
        or basis.shape != (2, 2)
        or triangle.shape != (3, 3)
        or not all(np.isfinite(a).all() for a in [origin, basis, triangle])
        or np.max(abs(basis.T @ basis - np.eye(2))) > 1e-12
        or np.linalg.det(basis) <= 0
    ):
        raise ValueError(
            "finite right-handed ramp frame and explicit ground triangle required"
        )
    normal = np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0])
    if normal[2] == 0:
        raise ValueError("nonvertical ramp ground plane required")
    local_triangles = np.concatenate(
        [
            shapely.get_coordinates(shapely.constrained_delaunay_triangles(z)).reshape(
                -1, 4, 2
            )[:, :3]
            for z in zones(half, landing_end)
        ]
    )
    local, inverse = np.unique(
        local_triangles.reshape(-1, 2), axis=0, return_inverse=True
    )
    xy = local @ basis.T + origin
    z = triangle[0, 2] - (xy - triangle[0, :2]) @ normal[:2] / normal[2]
    top_faces = inverse.reshape(-1, 3)
    p = xy[top_faces]
    u, v = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    negative = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0] < 0
    top_faces[negative] = top_faces[negative, ::-1]
    template = Mesh(np.c_[xy, z + relative_height(local, half, landing_end)], top_faces)
    shape = (
        outline(origin, basis, half, landing_end) if footprint is None else footprint
    )
    temporary, note = SupportVolumeBuilder(
        Mesh(triangle - [0, 0, 1], [[0, 1, 2]]),
        template,
        SupportBudget(maximum_footprint_coordinates=1024),
    ).build(shape, minimum_height_m=0.9999999)
    if temporary is None:
        raise ValueError(note["reason"])
    xy, faces = (
        temporary.upper_contact.vertices[:, :2],
        temporary.upper_contact.triangles,
    )
    z = triangle[0, 2] - (xy - triangle[0, :2]) @ normal[:2] / normal[2]
    heights = relative_height((xy - origin) @ basis, half, landing_end)
    lower, upper = np.c_[xy, z], np.c_[xy, z + heights]
    low, high = Mesh(lower, faces), Mesh(upper, faces)
    return Ramp(close_caps(high, low, method=method), low, high, shape)
