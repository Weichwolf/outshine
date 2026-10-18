"""Bound the unresolved roughness contribution of the isolated native MR fixture."""

import numpy as np


def field_envelope():
    bounds = []
    low = np.arange(16) / 16
    high = low + 1 / 16
    cells = np.arange(-1, 2)
    distance = np.maximum(
        np.maximum(
            (cells + 0.2)[None] - high[:, None], low[:, None] - (cells + 0.8)[None]
        ),
        0,
    )
    for sigma in (0.12, 0.24):
        # Each interval uses the minimum possible distance, not a sampled maximum.
        nearest = np.exp(-distance * distance / (2 * sigma * sigma)).sum(axis=1).max()
        tail = (
            2
            * np.exp(-(1.2**2) / (2 * sigma * sigma))
            / (1 - np.exp(-3.4 / (2 * sigma * sigma)))
        )
        bounds.append(1.25 * (nearest + tail + 1e-10) ** 2)
    return -0.25 * bounds[1], bounds[0]


def specular(roughness):
    a2 = np.asarray(roughness) ** 4
    fresnel = 0.04 + 0.96 * (1 - np.sqrt(0.9)) ** 5
    distribution = a2 / (np.pi * (0.1 + 0.9 * a2) ** 2)
    visibility = 0.5 / (0.8 + np.sqrt(0.64 + 0.36 * a2))
    return fresnel * distribution * visibility * 0.8


def lighting_interval(mean_albedo, authored_roughness):
    low, high = authored_roughness + 0.02 * np.asarray(field_envelope())
    # Above this maximum of D(a2), D and correlated Smith visibility both decrease.
    if not 0 < low < high < 1 or low**4 <= 0.1 / 0.9:
        raise ValueError("fixture is outside the derived monotone GGX interval")
    fresnel = 0.04 + 0.96 * (1 - np.sqrt(0.9)) ** 5
    diffuse = mean_albedo * (1 - fresnel) * 0.8 / np.pi
    return diffuse + specular(high), diffuse + specular(low)
