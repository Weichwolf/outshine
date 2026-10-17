"""Existing cubic/transport/sweep primitives across whole canonical dominant paths."""

import numpy as np
from limb_curve_data import path_data
from outshine_ref.vegetation import CubicSpine
from outshine_ref.vegetation.spine_frames import transport_frames
from outshine_ref.vegetation.spine_surface import sweep_stations
from outshine_ref.vegetation.tube_topology import close_rings
from scipy.interpolate import PchipInterpolator


def path_recipe(positions, radii, path):
    ids = np.asarray(path)
    if not 2 <= len(ids) <= 65:
        raise ValueError("two through65 source stations per cubic limb required")
    points, derivatives, radius = path_data(positions, radii, path)
    spine = CubicSpine.from_hermite(points, derivatives * (len(ids) - 1))
    profile = PchipInterpolator(np.linspace(0, 1, len(ids)), radius)
    return spine, profile


def station_budget(path_nodes, along, sides, max_faces, max_vertices, max_path=65):
    if (
        type(along) is not int
        or not 1 <= along <= 16
        or type(sides) is not int
        or not 3 <= sides <= 64
    ):
        raise ValueError("bounded explicit limb subdivisions required")
    if (
        type(max_faces) is not int
        or not 0 <= max_faces <= 262144
        or type(max_vertices) is not int
        or not 0 <= max_vertices <= 65536
    ):
        raise ValueError("bounded explicit output face and vertex capacities required")
    if not 2 <= path_nodes <= max_path:
        raise ValueError("bounded source stations per limb request required")
    stations = (path_nodes - 1) * along + 1
    if (
        stations > 1025
        or 2 * sides * stations > max_faces
        or stations * sides + 2 > max_vertices
    ):
        return None
    return stations


def limb_mesh(
    positions, radii, path, along=2, sides=8, *, max_faces=262144, max_vertices=65536
):
    stations = station_budget(len(path), along, sides, max_faces, max_vertices)
    if stations is None:
        return None
    spine, profile = path_recipe(positions, radii, path)
    parameter = np.linspace(0, 1, (len(path) - 1) * along + 1)
    centre, tangent, speed, curvature = spine.sample(parameter)
    return sampled_mesh(centre, tangent, speed, curvature, profile, parameter, sides)


def sampled_mesh(centre, tangent, speed, curvature, profile, parameter, sides):
    initial = np.eye(3)[np.argmin(abs(tangent[0]))]
    frames = transport_frames(centre, tangent, initial)
    radius = profile(parameter)
    if np.any(radius * np.linalg.norm(curvature, axis=1) >= 1):
        raise ValueError("sampled limb tube loses local regularity")
    surface, normals = sweep_stations(
        centre,
        tangent,
        speed,
        curvature,
        frames,
        radius,
        profile(parameter, 1),
        (1, 1),
        0,
        np.arange(sides) * 2 * np.pi / sides,
    )
    mesh = close_rings(surface, normals, centre, tangent)
    mesh["station_parameters"] = parameter
    return mesh
