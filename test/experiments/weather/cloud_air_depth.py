"""Depth reconstruction candidates for homogeneous shadowed air (#353)."""

import numpy as np
from cloud_air_transfer_reuse import coefficients


def optical_mass(distance_km, sigma):
    """Integral of clear camera transmission, in kilometres per RGB channel."""
    distance = np.asarray(distance_km, float)[..., None]
    sigma = np.asarray(sigma, float)
    return np.divide(
        -np.expm1(-distance * sigma),
        sigma,
        out=np.broadcast_to(distance, (*distance.shape[:-1], len(sigma))).copy(),
        where=sigma != 0,
    )


def solar_mass(samples, distances_km, rays, medium):
    """Recover J=integral(Tair*Csun ds) from native L=Direct*J+Psi*I."""
    sigma, direct, diffuse = coefficients(rays, medium)
    integral = optical_mass(distances_km, sigma)
    return (samples["radiance"] - diffuse * integral) / direct


def linear_source(nodes_km, point_solar, queries_km, sigma):
    """Integrate a bounded linear solar source with exact Beer attenuation."""
    nodes, queries = np.asarray(nodes_km), np.asarray(queries_km)
    widths = np.diff(nodes)
    if nodes[0] != 0 or np.any(widths <= 0):
        raise ValueError("source nodes must increase strictly from zero")
    if np.any(queries < 0) or np.any(queries > nodes[-1]):
        raise ValueError("queries must remain inside the physical air extent")
    if np.any(point_solar < 0) or np.any(point_solar > 1):
        raise ValueError("solar source must lie inside zero to one")
    changes = np.diff(point_solar)

    def partial(distance, cell):
        tau = distance[:, None] * sigma
        integral = optical_mass(distance, sigma)
        moment = np.empty_like(integral)
        thin = abs(tau) < 0.01
        series = distance[:, None] ** 2 * (
            0.5 + tau * (-1 / 3 + tau * (1 / 8 + tau * (-1 / 30 + tau / 144)))
        )
        np.divide(
            integral - distance[:, None] * np.exp(-tau),
            sigma,
            out=moment,
            where=~thin,
        )
        moment = np.where(thin, series, moment)
        return np.exp(-nodes[cell, None] * sigma) * (
            point_solar[cell, None] * integral
            + changes[cell, None] / widths[cell, None] * moment
        )

    prefix = np.r_[
        np.zeros((1, 3)), np.cumsum(partial(widths, np.arange(len(widths))), axis=0)
    ]
    index = np.minimum(
        np.searchsorted(nodes, queries, side="right") - 1, len(nodes) - 2
    )
    return prefix[index] + partial(queries - nodes[index], index)


def reconstruct(nodes_km, values, point_solar, queries_km, sigma, method):
    """Interpolate J; endpoint solar transfer is dJ/dI for Hermite only."""
    nodes = np.asarray(nodes_km, float)
    queries = np.asarray(queries_km, float)
    if np.any(np.diff(nodes) <= 0) or nodes[0] != 0:
        raise ValueError("depth nodes must increase strictly from zero")
    if np.any(queries < 0) or np.any(queries > nodes[-1]):
        raise ValueError("queries must remain inside the physical air extent")
    index = np.minimum(
        np.searchsorted(nodes, queries, side="right") - 1, len(nodes) - 2
    )
    fraction = ((queries - nodes[index]) / (nodes[index + 1] - nodes[index]))[:, None]
    mass = optical_mass(nodes, sigma)
    query_mass = optical_mass(queries, sigma)
    a, b = values[index], values[index + 1]
    if method in ("visibility", "legacy-visibility"):
        visibility = np.divide(values, mass, out=np.zeros_like(values), where=mass > 0)
        visibility[0] = point_solar[0] if method == "visibility" else 1
        return query_mass * (
            (1 - fraction) * visibility[index] + fraction * visibility[index + 1]
        )
    if method == "distance-linear":
        return (1 - fraction) * a + fraction * b
    width = mass[index + 1] - mass[index]
    t = np.divide(
        query_mass - mass[index], width, out=np.zeros_like(query_mass), where=width > 0
    )
    if method == "mass-linear":
        return (1 - t) * a + t * b
    if method != "mass-hermite":
        raise ValueError("unknown depth reconstruction")
    h00, h10 = 2 * t**3 - 3 * t**2 + 1, t**3 - 2 * t**2 + t
    h01, h11 = -2 * t**3 + 3 * t**2, t**3 - t**2
    return (
        h00 * a
        + h10 * width * point_solar[index, None]
        + h01 * b
        + h11 * width * point_solar[index + 1, None]
    )
