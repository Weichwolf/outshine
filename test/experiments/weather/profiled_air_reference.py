"""Independent variable-extinction RTE for native Earth-air source controls."""

import re
from pathlib import Path

import numpy as np
from cloud_air_depth_cases import solar_transfer
from cloud_air_reference import clear_span, cloud_cells, direction_for, origin_for
from cloud_air_source_reference import PSI, SOLAR, quadrature
from cloud_field import Layer, altitude, sphere_roots
from cloud_radiance import SUN


def earth_parameters():
    source = Path(__file__).resolve().parents[3] / "src/world/sky/Atmosphere.h"
    block = (
        source.read_text()
        .split("inline constexpr Medium kEarthAir = {", 1)[1]
        .split("};", 1)[0]
    )
    values = np.array(
        [
            float(x[:-1])
            for x in re.findall(r"[-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?f", block)
        ],
        "<f4",
    )
    if len(values) != 20:
        raise ValueError("the native Earth-air declaration must provide Medium80")
    return values


def extinction(points, medium):
    height_km = np.maximum(0, altitude(points, float(medium[0]) * 1000) * 0.001)
    rayleigh = np.exp(-height_km / float(medium[2]))
    mie = np.exp(-height_km / float(medium[3]))
    ozone = np.clip(1 - abs(height_km - float(medium[12])) / float(medium[13]), 0, 1)
    return (
        rayleigh[..., None] * medium[4:7].astype(float)
        + mie[..., None] * float(medium[11])
        + ozone[..., None] * medium[8:11].astype(float)
    ) * 0.001


def emission(points, ray, medium, solar):
    height_km = np.maximum(0, altitude(points, float(medium[0]) * 1000) * 0.001)
    rayleigh = np.exp(-height_km / float(medium[2]))[..., None] * medium[4:7].astype(
        float
    )
    mie = np.exp(-height_km / float(medium[3]))[..., None] * float(medium[7])
    cosine, g = float(ray["ray"][3]), float(medium[14])
    phase_r = 3 * (1 + cosine * cosine) / (16 * np.pi)
    phase_m = (
        3
        * (1 - g * g)
        * (1 + cosine * cosine)
        / (8 * np.pi * (2 + g * g) * (1 + g * g - 2 * g * cosine) ** 1.5)
    )
    return (
        SOLAR * (rayleigh * phase_r + mie * phase_m) * solar[..., None]
        + (rayleigh + mie) * PSI
    ) * 0.001


def reference(ray, queries_km, medium, cover, nodes=16, seconds=37, eye_cover=None):
    """Prescribed constant Sun/Psi LUTs; actual Earth profiles and Weather Sun-T6."""
    origin, direction = origin_for(ray, medium[0]), direction_for(ray)
    if (
        medium[0] != np.float32(6360)
        or ray["ray"][2] != np.float32(SUN[1])
        or abs(float(ray["ray"][3]) - np.dot(direction, SUN)) > 1e-6
    ):
        raise ValueError(
            "this control requires Earth radius and its declared daylight Sun"
        )
    begin, end = clear_span(
        origin,
        direction,
        float(ray["ray"][4]) * 1000,
        float(medium[0]) * 1000,
        float(medium[1]) * 1000,
    )
    if end <= begin:
        return np.tile([0, 0, 0, 1, 1, 1], (len(queries_km), 1)).astype(float)
    layer = Layer(cover=cover, radius_m=float(medium[0]) * 1000)
    eye = (
        layer if eye_cover is None else Layer(cover=eye_cover, radius_m=layer.radius_m)
    )
    cells = (
        cloud_cells(eye, origin, direction, end, seconds=seconds)
        if eye.cover > 0
        else []
    )
    queries = np.clip(np.asarray(queries_km, float) * 1000, 0, end)
    maximum = float(queries.max())
    cuts = []
    for height in (medium[12] - medium[13], medium[12], medium[12] + medium[13]):
        a, b, hit = sphere_roots(
            origin, direction, float(height) * 1000, layer.radius_m
        )
        if hit:
            cuts.extend((a, b))
    edges = np.unique(
        np.clip(
            np.r_[
                0,
                maximum,
                begin,
                end,
                np.arange(begin, maximum, 25),
                queries,
                cuts,
                [v for cell in cells for v in cell[:2]],
            ],
            0,
            maximum,
        )
    )
    a, b = edges[:-1], edges[1:]
    middle, half = (a + b) / 2, (b - a) / 2
    inside = (middle >= begin) & (middle < end)
    cloud_sigma = np.zeros(len(a))
    cloud_source = np.zeros((len(a), 3))
    for lo, hi, sigma, source in cells:
        active = (middle >= lo) & (middle < hi)
        cloud_sigma[active] += sigma
        cloud_source[active] += source
    x, w = quadrature(nodes)
    distance = middle[:, None] + half[:, None] * x
    points = origin + direction * distance[..., None]
    sigma = extinction(points, medium) * inside[:, None, None]
    full_tau = (
        half[:, None] * np.einsum("inc,n->ic", sigma, w)
        + cloud_sigma[:, None] * (b - a)[:, None]
    )
    prefix_tau = np.r_[np.zeros((1, 3)), np.cumsum(full_tau, axis=0)]
    inner_x, inner_w = quadrature(4)
    widths = (distance - a[:, None]) / 2
    inner_distance = a[:, None, None] + widths[..., None] * (1 + inner_x)
    inner_sigma = extinction(origin + direction * inner_distance[..., None], medium)
    local_tau = (
        widths[..., None]
        * np.einsum("inkc,k->inc", inner_sigma, inner_w)
        * inside[:, None, None]
    )
    local_tau += cloud_sigma[:, None, None] * (distance - a[:, None])[..., None]
    solar = (
        solar_transfer(points, cover, seconds) if cover > 0 else np.ones_like(distance)
    )
    source = (
        cloud_source[:, None, :]
        + emission(points, ray, medium, solar) * inside[:, None, None]
    )
    increments = half[:, None] * np.einsum(
        "inc,n->ic", source * np.exp(-prefix_tau[:-1, None, :] - local_tau), w
    )
    radiance = np.r_[np.zeros((1, 3)), np.cumsum(increments, axis=0)]
    at = np.searchsorted(edges, queries)
    return np.c_[radiance[at], np.exp(-prefix_tau[at])]
