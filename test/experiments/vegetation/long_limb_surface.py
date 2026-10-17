"""Bounded cubic pieces with a single radius profile, transported frame and skin."""

import numpy as np
from limb_curve_data import path_data
from limb_surface import limb_mesh, sampled_mesh, station_budget
from outshine_ref.vegetation import CubicSpine
from scipy.interpolate import PchipInterpolator


def piece_samples(positions, radii, path, along, chunk_edges=64):
    if type(along) is not int or not 1 <= along <= 16:
        raise ValueError("bounded explicit axial subdivision required")
    if type(chunk_edges) is not int or not 1 <= chunk_edges <= 64:
        raise ValueError("one through64 cubic intervals per piece required")
    points, derivatives, values = path_data(positions, radii, path)
    edges = len(points) - 1
    channels = [[] for _ in range(4)]
    for first in range(0, edges, chunk_edges):
        last = min(first + chunk_edges, edges)
        intervals = last - first
        spine = CubicSpine.from_hermite(
            points[first : last + 1], derivatives[first : last + 1] * intervals
        )
        local = np.linspace(0, 1, intervals * along + 1)
        keep = slice(None) if last == edges else slice(None, -1)
        for index, array in enumerate(spine.sample(local)):
            if index == 2:
                array = array * (edges / intervals)
            channels[index].append(array[keep])
    samples = [np.concatenate(parts) for parts in channels]
    profile = PchipInterpolator(np.linspace(0, 1, len(points)), values)
    return (*samples, profile)


def piecewise_limb_mesh(
    positions,
    radii,
    path,
    along=2,
    sides=8,
    *,
    max_faces=262144,
    max_vertices=65536,
    chunk_edges=64,
):
    stations = station_budget(
        len(path), along, sides, max_faces, max_vertices, max_path=1025
    )
    if stations is None:
        return None
    if len(path) <= 65 and chunk_edges == 64:
        return limb_mesh(
            positions,
            radii,
            path,
            along,
            sides,
            max_faces=max_faces,
            max_vertices=max_vertices,
        )
    centre, tangent, speed, curvature, profile = piece_samples(
        positions, radii, path, along, chunk_edges
    )
    parameter = np.linspace(0, 1, stations)
    return sampled_mesh(centre, tangent, speed, curvature, profile, parameter, sides)
