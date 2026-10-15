"""Exact shell and planetary-shadow boundaries for clear-air quadrature."""

import numpy as np


def column_edges(medium, radius, cosine, distance):
    radius, cosine, distance = np.broadcast_arrays(radius, cosine, distance)
    heights = medium.ozone_centre_km + medium.ozone_width_km * np.array([-1, 0, 1])
    shells = medium.bottom_km + heights
    projection = radius[..., None] * cosine[..., None]
    discriminant = projection**2 - radius[..., None] ** 2 + shells**2
    root = np.sqrt(np.maximum(0.0, discriminant))
    near = np.where(discriminant >= 0.0, -projection - root, 0.0)
    far = np.where(discriminant >= 0.0, -projection + root, 0.0)
    edges = np.concatenate(
        [np.zeros_like(distance)[..., None], near, far, distance[..., None]], axis=-1
    )
    return np.sort(np.clip(edges, 0.0, distance[..., None]), axis=-1)


def view_edges(medium, height_km, view_cos, sun_cos, relative_cos, distance_km):
    radius = medium.bottom_km + height_km
    edges = list(column_edges(medium, radius, view_cos, distance_km))
    view_sun = (
        view_cos * sun_cos
        + np.sqrt(max(0.0, 1.0 - view_cos**2))
        * np.sqrt(max(0.0, 1.0 - sun_cos**2))
        * relative_cos
    )
    a = 1.0 - view_sun**2
    b = 2.0 * radius * (view_cos - sun_cos * view_sun)
    c = radius**2 * (1.0 - sun_cos**2) - medium.bottom_km**2
    roots = []
    if abs(a) <= 1e-14:
        if abs(b) > 1e-14:
            roots = [-c / b]
    elif b * b - 4.0 * a * c >= 0.0:
        root = np.sqrt(b * b - 4.0 * a * c)
        stable = -0.5 * (b + np.copysign(root, b))
        roots = [stable / a, c / stable] if stable != 0.0 else [-b / (2.0 * a)]
    for position in roots:
        if (
            0.0 < position < distance_km
            and radius * sun_cos + position * view_sun < 0.0
        ):
            edges.append(position)
    return np.unique(edges)


def weighted_nodes(edges, nodes, weights):
    spans = np.diff(edges, axis=-1)
    positions = edges[..., :-1, None] + spans[..., None] * nodes
    weighted = spans[..., None] * weights
    shape = (*positions.shape[:-2], -1)
    return positions.reshape(shape), weighted.reshape(shape)
