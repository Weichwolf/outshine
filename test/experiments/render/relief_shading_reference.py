"""Clipped bevel pieces and triangle quadrature, independent of the subcell filter."""

import itertools

import numpy as np
from facade_footprint import screen_polygon
from masonry_relief_reference import pieces


def triangle_rule(shape, order):
    nodes, weights = np.polynomial.legendre.leggauss(order)
    nodes, weights = (nodes + 1) / 2, weights / 2
    u, v = np.meshgrid(nodes, nodes, indexing="ij")
    w = (weights[:, None] * weights[None, :] * (1 - u)).ravel()
    points, areas = [], []
    vertices = np.array(shape.exterior.coords[:-1])
    for index in range(1, len(vertices) - 1):
        a, b, c = vertices[[0, index, index + 1]]
        jacobian = abs(np.linalg.det(np.column_stack([b - a, c - a])))
        if jacobian == 0:
            continue
        points.append(
            a + u.ravel()[:, None] * (b - a) + ((1 - u) * v).ravel()[:, None] * (c - a)
        )
        areas.append(w * jacobian)
    return np.concatenate(points), np.concatenate(areas)


def distribution(at, dx, dy, order, depth=0.002, bevel=0.002):
    extent = (np.abs(dx) + np.abs(dy)) / 2
    low, high = at - extent - bevel / 2, at + extent + bevel / 2
    gradients, weights = [], []
    work = {"bricks": 0, "pieces": 0, "nonempty_pieces": 0, "quadrature_points": 0}
    for row in range(
        int(np.floor(low[1] / 0.08)) - 1, int(np.ceil(high[1] / 0.08)) + 1
    ):
        shift = (row % 2) * 0.125
        first = int(np.floor((low[0] - shift) / 0.25)) - 1
        last = int(np.ceil((high[0] - shift) / 0.25)) + 1
        for column in range(first, last + 1):
            x, y = column * 0.25 + shift, row * 0.08
            work["bricks"] += 1
            for horizontal, vertical in itertools.product(
                pieces(x + 0.005, x + 0.245, bevel),
                pieces(y + 0.005, y + 0.075, bevel),
            ):
                if horizontal[2] == vertical[2] == 0:
                    continue
                work["pieces"] += 1
                shape = screen_polygon(
                    at,
                    dx,
                    dy,
                    [horizontal[0], vertical[0]],
                    [horizontal[1], vertical[1]],
                )
                if shape.area < 1e-18:
                    continue
                point, area = triangle_rule(shape, order)
                material = at + point[:, :1] * dx + point[:, 1:] * dy
                u = horizontal[2] * (material[:, 0] - horizontal[0]) + horizontal[3]
                v = vertical[2] * (material[:, 1] - vertical[0]) + vertical[3]
                gradients.append(
                    depth * np.column_stack([horizontal[2] * v, vertical[2] * u])
                )
                weights.append(area)
                work["nonempty_pieces"] += 1
                work["quadrature_points"] += len(area)
    gradients.append(np.zeros((1, 2)))
    remainder = 1 - sum(float(np.sum(w)) for w in weights)
    if remainder < -1e-10:
        raise ValueError("overlapping reference pieces")
    weights.append(np.array([max(remainder, 0)]))
    return np.concatenate(gradients), np.concatenate(weights), work


def normals(gradient):
    value = np.column_stack([-gradient, np.ones(len(gradient))])
    return value / np.linalg.norm(value, axis=1)[:, None]


def direction(angle, azimuth=0):
    angle, azimuth = np.deg2rad([angle, azimuth])
    return np.array(
        [
            np.sin(angle) * np.cos(azimuth),
            np.sin(angle) * np.sin(azimuth),
            np.cos(angle),
        ]
    )


def specular(normal, roughness, light, view):
    """NumPy translation of metalRoughBrdf plus the subject nl/nv guards; F0=0.04."""
    nl = np.maximum(normal @ light, 0)
    nv = np.maximum(normal @ view, 1e-6)
    half = light + view
    half /= np.linalg.norm(half)
    nh = np.maximum(normal @ half, 0)
    vh = max(float(view @ half), 0)
    return specular_from_dots(roughness, nl, nv, nh, vh)


def specular_from_dots(roughness, nl, nv, nh, vh):
    """Independent GGX arithmetic with explicit dot products for native diagnostics."""
    a2 = np.asarray(roughness) ** 4
    distribution = a2 / (np.pi * (nh * nh * (a2 - 1) + 1) ** 2)
    visibility = 0.5 / (
        nl * np.sqrt(nv * nv * (1 - a2) + a2) + nv * np.sqrt(nl * nl * (1 - a2) + a2)
    )
    return (0.04 + 0.96 * (1 - vh) ** 5) * distribution * visibility * nl


def candidates(gradient, weights, roughness):
    mean = weights @ gradient
    normal = normals(mean[None])[0]
    unit_mean = weights @ normals(gradient)
    length = np.linalg.norm(unit_mean)
    covariance_trace = weights @ np.sum((gradient - mean) ** 2, axis=1)
    a2 = roughness**4
    d = length * a2 + 2 * (1 - a2) * (1 - length)
    n = 2 * length * (1 - a2)
    toksvig = min(2 * d / (n + 2 * d), 1) ** 0.25
    return {
        "mean_gradient": (normal, roughness),
        "unit_mean": (unit_mean / length, roughness),
        "toksvig": (unit_mean / length, toksvig),
        "slope_trace": (normal, min(a2 + covariance_trace, 1) ** 0.25),
    }


def mixture(gradient, weights):
    """At most nine sign-separated normal lobes; ideal offline statistics only."""
    labels = (
        (np.sign(gradient[:, 0]).astype(int) + 1) * 3
        + np.sign(gradient[:, 1]).astype(int)
        + 1
    )
    unit = normals(gradient)
    lobes, coverage = [], []
    for label in range(9):
        selected = labels == label
        area = weights[selected].sum()
        if area <= 1e-15:
            continue
        mean = weights[selected] @ unit[selected] / area
        lobes.append(mean / np.linalg.norm(mean))
        coverage.append(area)
    return np.array(lobes), np.array(coverage)
