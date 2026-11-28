"""Analytic elliptical, rhombic and sector cross sections with one-sided tangents."""

import numpy as np


def section_vertex_count(kind, sides):
    return 4 if kind == "rhombus" else sides // 2 + 2 if kind == "sector" else sides


def section_sampling(kind, sides):
    if kind == "sector":
        angles = np.concatenate(
            ([0.0], np.linspace(np.pi / 2, 3 * np.pi / 2, sides // 2 + 1))
        )
        following = np.roll(angles, -1)
        middle = angles + np.remainder(following - angles, 2 * np.pi) / 2
        tags = np.where(middle < np.pi / 2, 0, np.where(middle < 3 * np.pi / 2, 1, 2))
        return angles, middle, tags
    count = section_vertex_count(kind, sides)
    angles = np.arange(count) * 2 * np.pi / count
    return (
        angles,
        (np.arange(count) + 0.5) * 2 * np.pi / count,
        np.arange(count) if kind == "rhombus" else None,
    )


def section_centroid(kind, aspect, count):
    alpha = 2 * np.pi / count
    return np.array(
        [0.0, aspect * 4 * np.sin(alpha / 2) / (3 * alpha) if kind == "sector" else 0.0]
    )


def section_field(kind, aspect, count, angle, edge=None):
    angle = np.asarray(angle, float)
    if kind == "ellipse":
        return np.stack((np.cos(angle), aspect * np.sin(angle)), axis=-1), np.stack(
            (-np.sin(angle), aspect * np.cos(angle)), axis=-1
        )
    if kind == "rhombus":
        coordinate = np.remainder(angle, 2 * np.pi) / (np.pi / 2)
        index = (
            np.floor(coordinate).astype(int)
            if edge is None
            else np.asarray(edge, int) % 4
        )
        corners = np.array(((1, 0), (0, aspect), (-1, 0), (0, -aspect)))
        following = corners[(index + 1) % 4] - corners[index]
        fraction = np.remainder(coordinate - index, 4)
        return corners[index] + fraction[..., None] * following, following / (np.pi / 2)
    phi = np.remainder(angle, 2 * np.pi)
    alpha = 2 * np.pi / count
    beta = (np.pi - alpha) / 2
    start = np.array([np.cos(beta), aspect * np.sin(beta)])
    end = np.array([np.cos(beta + alpha), aspect * np.sin(beta + alpha)])
    start[np.abs(start) < 1e-15] = 0
    end[np.abs(end) < 1e-15] = 0
    tags = (
        np.where(phi < np.pi / 2, 0, np.where(phi < 3 * np.pi / 2, 1, 2))
        if edge is None
        else np.asarray(edge, int)
    )
    if np.any((tags < 0) | (tags > 2)):
        raise ValueError("declared one-sided sector boundary required")
    theta = beta + (phi - np.pi / 2) * alpha / np.pi
    arc = np.stack((np.cos(theta), aspect * np.sin(theta)), axis=-1)
    arc_d = np.stack((-np.sin(theta), aspect * np.cos(theta)), axis=-1) * alpha / np.pi
    out = (phi / (np.pi / 2))[..., None] * start
    back = ((2 * np.pi - phi) / (np.pi / 2))[..., None] * end
    back = np.where((phi == 0)[..., None], 0, back)
    point = np.where(
        (tags == 0)[..., None], out, np.where((tags == 1)[..., None], arc, back)
    )
    derivative = np.where(
        (tags == 0)[..., None],
        start / (np.pi / 2),
        np.where((tags == 1)[..., None], arc_d, -end / (np.pi / 2)),
    )
    return point, derivative
