"""Independent RTE integration of spatial Weather shadows and declared cloud cells."""

from dataclasses import replace
from functools import cache

import numpy as np
from cloud_air_reference import (
    RAY,
    clear_span,
    cloud_cells,
    direction_for,
    origin_for,
)
from cloud_field import Layer, optical_depth
from cloud_radiance import SUN

SOLAR = np.array([0.75, 0.5, 0.25])
PSI = np.array([1 / 64, 1 / 32, 1 / 16])


@cache
def quadrature(count):
    nodes, weights = np.polynomial.legendre.leggauss(count)
    nodes.flags.writeable = weights.flags.writeable = False
    return nodes, weights


def medium_parameters():
    return np.array(
        [
            6360,
            6460,
            1e20,
            1e20,
            0.01,
            0.02,
            0.03,
            0.003,
            0,
            0,
            0,
            0.004,
            25,
            15,
            0.8,
            0,
            0.1,
            0.13,
            0.07,
            0,
        ],
        dtype="<f4",
    )


def coefficients(ray, medium):
    cosine, g = float(ray["ray"][3]), float(medium[14])
    rayleigh = medium[4:7].astype(float)
    mie = float(medium[7])
    phase_r = 3 * (1 + cosine * cosine) / (16 * np.pi)
    phase_m = (
        3
        * (1 - g * g)
        * (1 + cosine * cosine)
        / (8 * np.pi * (2 + g * g) * (1 + g * g - 2 * g * cosine) ** 1.5)
    )
    return (
        (rayleigh + float(medium[11])) * 0.001,
        SOLAR * (rayleigh * phase_r + mie * phase_m) * 0.001,
        (rayleigh + mie) * PSI * 0.001,
    )


def inputs(sparse=False):
    rows, values = [], []
    poses = [
        (eye, cosine, azimuth)
        for eye in (20, 1772, 2800, 4000)
        for cosine in (-0.1, 0, 0.1)
        for azimuth in (0, np.pi)
    ] + [(110000, cosine, 0) for cosine in (-1, -0.95)]
    for eye, cosine, azimuth in poses:
        ray = np.zeros((), RAY)
        ray["ray"] = [6360 + eye / 1000, cosine, 0.8, 0, 10000]
        ray["azimuth"] = azimuth
        ray["ray"][3] = np.dot(direction_for(ray), SUN)
        maximum = 120 if eye > 100000 else 20
        distances = np.linspace(0, maximum, 81, dtype="<f4")
        if sparse:
            distances = distances[::8]
        distances = np.repeat(distances, 2)
        ray["begin"] = len(values)
        values.extend(distances)
        ray["end"] = len(values)
        rows.append(ray)
    return np.array(rows, RAY), np.asarray(values, "<f4")


def reference_ray(ray, queries_km, medium, cover, nodes=16, air_sun_transfer=None):
    if np.any(medium[2:4] < 1e19) or np.any(medium[8:11] != 0):
        raise ValueError("the independent control requires homogeneous clear air")
    origin, direction = origin_for(ray, medium[0]), direction_for(ray)
    begin, end = clear_span(
        origin, direction, 1e7, float(medium[0]) * 1000, float(medium[1]) * 1000
    )
    layer = replace(Layer(), cover=cover)
    cells = cloud_cells(layer, origin, direction, end) if cover > 0 else []
    queries = np.clip(np.asarray(queries_km, float) * 1000, 0, end)
    maximum = float(queries.max())
    fixed = np.arange(begin, maximum, 25)
    edges = np.unique(
        np.clip(
            np.r_[
                0,
                maximum,
                begin,
                end,
                fixed,
                queries,
                [x for c in cells for x in c[:2]],
            ],
            0,
            maximum,
        )
    )
    first, last = edges[:-1], edges[1:]
    half, middle = (last - first) / 2, (last + first) / 2
    inside = (middle >= begin) & (middle < end)
    sigma, direct, diffuse = coefficients(ray, medium)
    extinction = np.where(inside[:, None], sigma, 0.0)
    emission = np.zeros((len(first), 3))
    for a, b, cloud_sigma, cloud_emission in cells:
        hit = (middle >= a) & (middle < b)
        extinction[hit] += cloud_sigma
        emission[hit] += cloud_emission
    tau = extinction * (last - first)[:, None]
    prefix_tau = np.r_[np.zeros((1, 3)), np.cumsum(tau, axis=0)]
    x, w = quadrature(nodes)
    distance = middle[:, None] + half[:, None] * x
    points = origin + direction * distance[..., None]
    sun_t = (
        np.ones_like(distance)
        if cover <= 0
        else (
            air_sun_transfer(points)
            if air_sun_transfer is not None
            else np.exp(-optical_depth(points, SUN, layer, 6, 37, maximum_m=1e8))
        )
    )
    source = emission[:, None, :] + inside[:, None, None] * (
        direct * sun_t[..., None] + diffuse
    )
    attenuated = source * np.exp(
        -prefix_tau[:-1, None, :]
        - extinction[:, None, :] * (distance - first[:, None])[..., None]
    )
    increments = half[:, None] * np.einsum("ijc,j->ic", attenuated, w)
    cumulative = np.r_[np.zeros((1, 3)), np.cumsum(increments, axis=0)]
    indices = np.searchsorted(edges, queries)
    return np.c_[cumulative[indices], np.exp(-prefix_tau[indices])]


def reference(rays, distances, medium, cover, nodes=16):
    return np.concatenate(
        [
            reference_ray(
                ray,
                distances[int(ray["begin"]) : int(ray["end"])],
                medium,
                cover,
                nodes,
            )
            for ray in rays
        ]
    )
