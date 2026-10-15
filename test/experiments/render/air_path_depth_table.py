"""Prefix-integrated depth columns; angular interpolation remains outside this experiment."""

import numpy as np
from air_path_integral import (
    boundary_distance,
    hits_ground,
    quadrature,
    source,
    transmittance,
)
from air_path_segments import view_edges, weighted_nodes


def depth_limit(medium, height_km, view_cos, maximum_km):
    radius = medium.bottom_km + height_km
    limit = min(maximum_km, boundary_distance(radius, view_cos, medium.top_km))
    if hits_ground(medium, radius, view_cos):
        discriminant = radius**2 * (view_cos**2 - 1.0) + medium.bottom_km**2
        limit = min(limit, -radius * view_cos - np.sqrt(max(0.0, discriminant)))
    return float(limit)


def depth_nodes(count, limit, mapping):
    unit = np.linspace(0.0, 1.0, count)
    return unit * limit if mapping == "linear" else unit**2 * limit


def build_column(medium, arguments, count, mapping, samples=4, half_storage=False):
    height, view_cos = arguments[:2]
    limit = depth_limit(medium, height, view_cos, 200.0)
    nodes = depth_nodes(count, limit, mapping)
    edges = np.unique(np.concatenate([nodes, view_edges(medium, *arguments, limit)]))
    positions, weighted = weighted_nodes(edges, *quadrature(samples))
    _, scattered = source(medium, *arguments, positions)
    throughput = transmittance(medium, medium.bottom_km + height, view_cos, positions)
    contributions = (scattered * throughput * weighted[:, None]).reshape(-1, samples, 3)
    cumulative = np.concatenate(
        [np.zeros((1, 3)), np.cumsum(np.sum(contributions, axis=1), axis=0)]
    )
    radiance = cumulative[np.searchsorted(edges, nodes)]
    through = transmittance(medium, medium.bottom_km + height, view_cos, nodes)
    if half_storage:
        radiance = radiance.astype(np.float16).astype(np.float64)
        through = through.astype(np.float16).astype(np.float64)
    return radiance, through, limit, len(positions)


def sample_column(column, distance_km, mapping):
    radiance, through, limit, _ = column
    unit = np.clip(distance_km / limit, 0.0, 1.0)
    coordinate = unit if mapping == "linear" else np.sqrt(unit)
    index = coordinate * (len(radiance) - 1)
    low = min(int(np.floor(index)), len(radiance) - 2)
    edges = depth_nodes(len(radiance), limit, mapping)
    fraction = (distance_km - edges[low]) / (edges[low + 1] - edges[low])
    fraction = np.clip(fraction, 0.0, 1.0)
    return tuple(
        values[low] * (1.0 - fraction) + values[low + 1] * fraction
        for values in (radiance, through)
    )
