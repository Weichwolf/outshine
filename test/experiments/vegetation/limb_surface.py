"""Existing cubic/transport/sweep primitives across whole canonical dominant paths."""

import numpy as np
from limb_spine import LimbSpine
from outshine_ref.vegetation import CubicSpine
from outshine_ref.vegetation.spine_frames import transport_frames, unit
from outshine_ref.vegetation.spine_surface import sweep_stations
from outshine_ref.vegetation.tube_topology import close_rings
from scipy.interpolate import PchipInterpolator


def path_recipe(positions, radii, path):
    ids = np.asarray(path)
    if not 2 <= len(ids) <= 1025:
        raise ValueError("two through1025 source stations per cubic limb required")
    points = positions[ids]
    chord = np.diff(points, axis=0)
    lengths = np.linalg.norm(chord, axis=1)
    tangent = unit(chord)
    derivatives = np.empty_like(points)
    derivatives[0] = tangent[0] * lengths[0]
    derivatives[-1] = tangent[-1] * lengths[-1]
    if len(ids) > 2:
        derivatives[1:-1] = (
            unit(tangent[:-1] + tangent[1:])
            * np.minimum(lengths[:-1], lengths[1:])[:, None]
        )
    spine = (
        CubicSpine.from_hermite(points, derivatives * (len(ids) - 1))
        if len(ids) <= 65
        else LimbSpine(points, derivatives)
    )
    radius = radii[ids].copy()
    if ids[0] != 0:
        radius[0] = radius[1]
    radius[-1] *= 0.35
    profile = PchipInterpolator(np.linspace(0, 1, len(ids)), radius)
    return spine, profile


def limb_mesh(
    positions,
    radii,
    path,
    along=2,
    sides=8,
    *,
    max_faces=262144,
    max_vertices=65536,
    bark=False,
):
    if (
        type(bark) is not bool
        or type(along) is not int
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
    if not 2 <= len(path) <= 1025:
        raise ValueError("two through1025 source stations per cubic limb required")
    stations = (len(path) - 1) * along + 1
    if 2 * sides * stations > max_faces or stations * sides + 2 > max_vertices:
        return None
    spine, profile = path_recipe(positions, radii, path)
    parameter = np.linspace(0, 1, (len(path) - 1) * along + 1)
    centre, tangent, speed, curvature = spine.sample(parameter)
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
    if bark:
        from limb_bark import bark_fields

        mesh.update(bark_fields(spine, parameter, radius, sides))
    return mesh
