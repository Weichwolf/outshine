"""Atmospheric density and extinction for matched clear and hazy views."""

import numpy as np

EARTH_RADIUS_KM = 6360.0
ATMOSPHERE_TOP_KM = 100.0
RAYLEIGH_HEIGHT_KM = 8.0
RAYLEIGH_SCATTERING_PER_KM = (0.005802, 0.013558, 0.033100)
AEROSOL_HEIGHT_KM = 1.2
AEROSOL_COLUMN_HEIGHT_KM = 12 * AEROSOL_HEIGHT_KM
OZONE_CENTRE_KM = 25.0
OZONE_HALF_WIDTH_KM = 15.0
OZONE_ABSORPTION_PER_KM = (0.000650, 0.001881, 0.000085)
nodes, weights = np.polynomial.legendre.leggauss(6)
COLUMN_SAMPLES = tuple(zip(0.5 * (1 + nodes), 0.5 * weights, strict=True))
COLUMN_NODES, COLUMN_WEIGHTS = np.polynomial.legendre.leggauss(12)


def aerosol_extinction(weather):
    try:
        return {"clear": 0.0, "haze": 0.04, "broken-cloud": 0.0, "overcast": 0.0}[
            weather
        ]
    except KeyError as error:
        raise ValueError(f"unsupported reference weather {weather!r}") from error


def direct_sun_transmission(eye_m, sun_vertical, aerosol):
    """Integrate the reference atmosphere along the ray toward the sun."""
    height = max(0.0, eye_m * 0.001)
    radius = EARTH_RADIUS_KM + height
    vertical = radius * sun_vertical
    if vertical < 0 and vertical**2 >= height * (radius + EARTH_RADIUS_KM):
        return (0.0, 0.0, 0.0)
    # Split the integral at ozone's changes of slope and at the lowest point of
    # a dipping ray. Each segment then has smooth densities.
    levels = np.array(
        [
            OZONE_CENTRE_KM - OZONE_HALF_WIDTH_KM,
            OZONE_CENTRE_KM,
            OZONE_CENTRE_KM + OZONE_HALF_WIDTH_KM,
            ATMOSPHERE_TOP_KM,
        ]
    )
    radial = (levels - height) * (2 * EARTH_RADIUS_KM + levels + height)
    discriminants = vertical**2 + radial
    if discriminants[-1] <= 0:
        return (1.0, 1.0, 1.0)
    root = np.sqrt(np.maximum(discriminants, 0))
    start = max(0.0, -vertical - root[-1])
    reach = -vertical + root[-1]
    if reach <= 0:
        return (1.0, 1.0, 1.0)
    boundaries = np.r_[start, -vertical - root, -vertical + root, -vertical]
    edges = np.sort(np.clip(boundaries, start, reach))
    left, right = edges[:-1], edges[1:]
    half = 0.5 * (right - left)
    distance = left[:, None] + half[:, None] * (1 + COLUMN_NODES)
    delta = distance * (2 * vertical + distance)
    altitude = height + delta / (np.sqrt(radius**2 + delta) + radius)
    rayleigh = np.exp(-altitude / RAYLEIGH_HEIGHT_KM)
    mie = np.exp(-altitude / AEROSOL_HEIGHT_KM)
    ozone = np.maximum(0.0, 1 - abs(altitude - OZONE_CENTRE_KM) / OZONE_HALF_WIDTH_KM)
    depth = (
        (half @ (rayleigh @ COLUMN_WEIGHTS)) * np.array(RAYLEIGH_SCATTERING_PER_KM)
        + (half @ (mie @ COLUMN_WEIGHTS)) * aerosol
        + (half @ (ozone @ COLUMN_WEIGHTS)) * np.array(OZONE_ABSORPTION_PER_KM)
    )
    return tuple(np.exp(-depth))
