"""WGS84 coordinates for an explicitly bound model-height mapping."""

import math

import numpy as np


def frame(anchor):
    lon, lat = map(float, anchor)
    if (
        not math.isfinite(lon)
        or not math.isfinite(lat)
        or not -180 <= lon <= 180
        or not -90 <= lat <= 90
    ):
        raise ValueError("finite WGS84 longitude/latitude required")
    lon, lat = np.deg2rad([lon, lat])
    sl, cl, sb, cb = math.sin(lon), math.cos(lon), math.sin(lat), math.cos(lat)
    a = 6378137.0
    f = 1 / 298.257223563
    e2 = f * (2 - f)
    n = a / math.sqrt(1 - e2 * sb * sb)
    origin = np.array([n * cb * cl, n * cb * sl, n * (1 - e2) * sb])
    axes = np.array([[-sl, -sb * cl, cb * cl], [cl, -sb * sl, cb * sl], [0, cb, sb]])
    return origin, axes


A = 6378137.0
F = 1 / 298.257223563
E2 = F * (2 - F)


def positions_in_frame(anchor, geographic, heights):
    geographic = np.asarray(geographic, float)
    heights = np.asarray(heights, float)
    if (
        geographic.ndim != 2
        or geographic.shape[1] != 2
        or heights.shape != (len(geographic),)
    ):
        raise ValueError("one explicit model height per geographic point required")
    if (
        not np.isfinite(geographic).all()
        or not np.isfinite(heights).all()
        or (abs(geographic) > [180, 90]).any()
    ):
        raise ValueError("bounded finite model geographic positions required")
    lon, lat = np.deg2rad(geographic).T
    normal = A / np.sqrt(1 - E2 * np.sin(lat) ** 2)
    points = np.c_[
        (normal + heights) * np.cos(lat) * np.cos(lon),
        (normal + heights) * np.cos(lat) * np.sin(lon),
        (normal * (1 - E2) + heights) * np.sin(lat),
    ]
    origin, axes = frame(anchor)
    return (points - origin) @ axes


def ground_geographic_at(anchor, xy):
    xy = np.asarray(xy, float)
    if (
        xy.ndim != 2
        or xy.shape[1] != 2
        or not np.isfinite(xy).all()
        or abs(xy).max() > 1e6
    ):
        raise ValueError("bounded source-frame ground queries required")
    origin, axes = frame(anchor)
    base = origin + np.c_[xy, np.zeros(len(xy))] @ axes.T
    up = axes[:, 2]
    q = np.array([1 / A**2, 1 / A**2, 1 / (A * (1 - F)) ** 2])
    quadratic = float(np.dot(up * q, up))
    linear = 2 * (base * q) @ up
    constant = np.sum(base**2 * q, axis=1) - 1
    discriminant = linear**2 - 4 * quadratic * constant
    if (discriminant <= 0).any():
        raise ValueError("source-frame ground ray misses ellipsoid")
    z = -2 * constant / (linear + np.sqrt(discriminant))
    ground = base + z[:, None] * up
    geographic = np.rad2deg(
        np.c_[
            np.arctan2(ground[:, 1], ground[:, 0]),
            np.arctan2(ground[:, 2], (1 - E2) * np.hypot(ground[:, 0], ground[:, 1])),
        ]
    )
    return geographic
