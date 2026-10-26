"""Independent spherical entry and bilinear LUT lookup for the visible solar disc."""

import numpy as np

BACKGROUND = np.array([0.125, 0.25, 0.5])


def directions(parameters, width, height):
    p = np.asarray(parameters, float)
    x = (np.arange(width) + 0.5) * 2 / width - 1
    y = 1 - (np.arange(height) + 0.5) * 2 / height
    xx, yy = np.meshgrid(x, y)
    rays = p[8:11] + xx[..., None] * p[20] * p[:3] + yy[..., None] * p[21] * p[4:7]
    return rays / np.linalg.norm(rays, axis=-1)[..., None]


def bilinear(lut, u, v):
    height, width = lut.shape[:2]
    x, y = (
        np.clip(u * width - 0.5, 0, width - 1),
        np.clip(v * height - 0.5, 0, height - 1),
    )
    a, b = np.floor(x).astype(int), np.floor(y).astype(int)
    c, d = np.minimum(a + 1, width - 1), np.minimum(b + 1, height - 1)
    fx, fy = (x - a)[..., None], (y - b)[..., None]
    return (1 - fy) * ((1 - fx) * lut[b, a] + fx * lut[b, c]) + fy * (
        (1 - fx) * lut[d, a] + fx * lut[d, c]
    )


def transfer(parameters, rays, lut, *, endpoints=True):
    p = np.asarray(parameters, float)
    radius, bottom, top = p[23], p[28], p[29]
    cosine = rays[..., 1]
    projection = radius * cosine
    discriminant = projection**2 + (top - radius) * (top + radius)
    crosses = (
        (cosine < 0) & (discriminant > 0)
        if radius > top
        else np.ones_like(cosine, bool)
    )
    at_radius = min(radius, top)
    at_cosine = -np.sqrt(np.maximum(discriminant, 0)) / top if radius > top else cosine
    span = np.sqrt((top - bottom) * (top + bottom))
    ground = np.sqrt((at_radius - bottom) * (at_radius + bottom))
    reach = -at_radius * at_cosine + np.sqrt(
        np.maximum(
            0, (at_radius * at_cosine) ** 2 + (top - at_radius) * (top + at_radius)
        )
    )
    shortest, longest = top - at_radius, ground + span
    u = (reach - shortest) / (longest - shortest)
    v = np.full_like(cosine, ground / span)
    if endpoints:
        height, width = lut.shape[:2]
        u = (u * (width - 1) + 0.5) / width
        v = (v * (height - 1) + 0.5) / height
    value = bilinear(lut, u, v)
    value = np.where(crosses[..., None], value, 1)
    blocked = (cosine <= 0) & (projection**2 >= (radius - bottom) * (radius + bottom))
    return np.where(blocked[..., None], 0, value), ~blocked


def reference(parameters, width, height, lut, *, endpoints=True):
    p = np.asarray(parameters, float)
    rays = directions(p, width, height)
    through, visible = transfer(p, rays, lut, endpoints=endpoints)
    sun = p[16:19] / np.linalg.norm(p[16:19])
    dot = rays @ sun
    disc = dot > np.cos(p[26])
    rgb = through * (p[22] / (np.pi * p[26] ** 2))
    return (
        np.where(disc[..., None], rgb, 0) + BACKGROUND * p[22],
        dot - np.cos(p[26]),
        visible,
    )
