"""Atmospheric density and extinction for matched clear and hazy views."""

import math

import numpy as np

EARTH_RADIUS_KM = 6360.0
RAYLEIGH_HEIGHT_KM = 8.0
RAYLEIGH_SCATTERING_PER_KM = (0.005802, 0.013558, 0.033100)
AEROSOL_HEIGHT_KM = 1.2
AEROSOL_COLUMN_HEIGHT_KM = 12 * AEROSOL_HEIGHT_KM
nodes, weights = np.polynomial.legendre.leggauss(6)
COLUMN_SAMPLES = tuple(zip(0.5 * (1 + nodes), 0.5 * weights, strict=True))


def aerosol_extinction(weather):
    try:
        return {"clear": 0.0, "haze": 0.04, "broken-cloud": 0.0, "overcast": 0.0}[
            weather
        ]
    except KeyError as error:
        raise ValueError(f"unsupported reference weather {weather!r}") from error


def direct_sun_fraction(eye_m, sun_vertical, extinction):
    if extinction == 0:
        return 1.0
    if sun_vertical <= 0:
        return 0.0
    height = eye_m * 0.001
    radius = EARTH_RADIUS_KM + height
    top = radius + AEROSOL_COLUMN_HEIGHT_KM
    radial = (top - radius) * (top + radius)
    vertical = radius * sun_vertical
    distance = radial / (math.sqrt(vertical**2 + radial) + vertical)
    column = 0.0
    for fraction, weight in COLUMN_SAMPLES:
        step = distance * fraction
        delta = step * (2 * vertical + step)
        altitude = height + delta / (math.sqrt(radius**2 + delta) + radius)
        column += weight * math.exp(-max(0.0, altitude) / AEROSOL_HEIGHT_KM)
    return math.exp(-extinction * distance * column)
