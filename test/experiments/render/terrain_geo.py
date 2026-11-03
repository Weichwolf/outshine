"""WGS84 frames shared by terrain experiments."""

import numpy as np


def ecef(lon, lat, height=0):
    lon, lat = np.deg2rad(lon), np.deg2rad(lat)
    n = 6378137 / np.sqrt(1 - 0.0066943799901413165 * np.sin(lat) ** 2)
    return np.stack(
        (
            (n + height) * np.cos(lat) * np.cos(lon),
            (n + height) * np.cos(lat) * np.sin(lon),
            (n * (1 - 0.0066943799901413165) + height) * np.sin(lat),
        ),
        axis=-1,
    )


def page_frame(row):
    z, x, y = (int(row[k]) for k in ("zoom", "x", "y"))
    scale = 2**z
    lon = np.array((x, x + 1)) / scale * 360 - 180
    lat = np.rad2deg(np.arctan(np.sinh(np.pi * (1 - 2 * np.array((y, y + 1)) / scale))))
    mid_lon, mid_lat = lon.mean(), lat.mean()
    l, p = np.deg2rad((mid_lon, mid_lat))
    axes = np.array(
        (
            (-np.sin(l), np.cos(l), 0),
            (-np.sin(p) * np.cos(l), -np.sin(p) * np.sin(l), np.cos(p)),
            (np.cos(p) * np.cos(l), np.cos(p) * np.sin(l), np.sin(p)),
        )
    )
    centre = ecef(mid_lon, mid_lat)
    corners = np.einsum(
        "ij,kj->ik", ecef(lon[[0, 1, 0, 1]], lat[[0, 0, 1, 1]]) - centre, axes[:2]
    )
    return centre, axes, corners.astype("float32").astype(float)


