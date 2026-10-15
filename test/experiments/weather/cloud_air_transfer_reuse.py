"""Separate weighted solar visibility from local homogeneous-air response (#353)."""

import numpy as np
from cloud_air_source_reference import PSI, SOLAR


def coefficients(rays, medium):
    if np.any(medium[2:4] < 1e19) or np.any(medium[8:11] != 0):
        raise ValueError("this transfer control requires homogeneous clear air")
    cosine = rays["ray"][:, 3].astype(float)
    g = float(medium[14])
    rayleigh, mie = medium[4:7].astype(float), float(medium[7])
    phase_r = 3 * (1 + cosine**2) / (16 * np.pi)
    phase_m = (
        3
        * (1 - g * g)
        * (1 + cosine**2)
        / (8 * np.pi * (2 + g * g) * (1 + g * g - 2 * g * cosine) ** 1.5)
    )
    sigma = rayleigh + float(medium[11])
    direct = SOLAR * (rayleigh * phase_r[:, None] + mie * phase_m[:, None])
    diffuse = (rayleigh + mie) * PSI
    return sigma, direct, diffuse


def clear_response(rays, queries_km, medium):
    radius = rays["ray"][:, 0].astype(float)
    cosine = rays["ray"][:, 1].astype(float)
    projection = radius * cosine
    top = float(medium[1])
    outer = np.sqrt(np.maximum(0, projection**2 + (top - radius) * (top + radius)))
    begin = np.maximum(0, -projection - outer)
    end = np.minimum(rays["ray"][:, 4], -projection + outer)
    bottom = float(medium[0])
    discriminant = projection**2 + (bottom - radius) * (bottom + radius)
    ground = -projection - np.sqrt(np.maximum(discriminant, 0))
    blocks = (cosine < 0) & (discriminant >= 0) & (ground >= 0)
    end = np.minimum(end, np.where(blocks, ground, np.inf))
    length_km = np.maximum(0, np.minimum(queries_km, end) - begin)
    sigma, direct, diffuse = coefficients(rays, medium)
    tau = length_km[:, None] * sigma
    through = np.exp(-tau)
    integral_km = -np.expm1(-tau) / sigma
    return integral_km, through, direct, diffuse


def weighted_visibility(samples, rays, queries_km, medium):
    integral, _, direct, diffuse = clear_response(rays, queries_km, medium)
    result = np.ones_like(integral)
    np.divide(samples["radiance"], integral, out=result, where=integral > 0)
    result -= diffuse
    np.divide(result, direct, out=result, where=direct > 0)
    return np.where((integral > 0) & (direct > 0), result, 1)


def axis_nodes(length, step):
    if length < 2 or step < 1:
        raise ValueError("an angular grid needs positive spacing and two endpoints")
    return np.r_[np.arange(0, length - 1, step), length - 1]


def grid_indices(width, height, step):
    x, y = axis_nodes(width, step), axis_nodes(height, step)
    return (y[:, None] * width + x[None, :]).ravel(), (len(y), len(x))


def bilinear(values, width, height, step):
    x, y = axis_nodes(width, step), axis_nodes(height, step)
    ix = np.minimum(np.arange(width) // step, len(x) - 2)
    iy = np.minimum(np.arange(height) // step, len(y) - 2)
    tx = ((np.arange(width) - x[ix]) / (x[ix + 1] - x[ix]))[None, :, None]
    ty = ((np.arange(height) - y[iy]) / (y[iy + 1] - y[iy]))[:, None, None]
    a = values[iy[:, None], ix[None, :]]
    b = values[iy[:, None], ix[None, :] + 1]
    c = values[iy[:, None] + 1, ix[None, :]]
    d = values[iy[:, None] + 1, ix[None, :] + 1]
    return (1 - ty) * ((1 - tx) * a + tx * b) + ty * ((1 - tx) * c + tx * d)


def cubic_axis(length, step):
    nodes = axis_nodes(length, step)
    positions = np.arange(length)
    cell = np.minimum(positions // step, len(nodes) - 2)
    indices = np.clip(cell[:, None] + np.arange(-1, 3), 0, len(nodes) - 1)
    coordinate = nodes[indices]
    spacing = coordinate[:, 2] - coordinate[:, 1]
    t = (positions - coordinate[:, 1]) / spacing
    h00, h10 = 2 * t**3 - 3 * t**2 + 1, t**3 - 2 * t**2 + t
    h01, h11 = -2 * t**3 + 3 * t**2, t**3 - t**2
    before = spacing / (coordinate[:, 2] - coordinate[:, 0])
    after = spacing / (coordinate[:, 3] - coordinate[:, 1])
    weights = np.stack(
        [-h10 * before, h00 - h11 * after, h01 + h10 * before, h11 * after], axis=-1
    )
    return indices, weights


def cubic(values, width, height, step):
    x, wx = cubic_axis(width, step)
    y, wy = cubic_axis(height, step)
    result = np.zeros((height, width, values.shape[-1]))
    for row in range(4):
        for column in range(4):
            result += (
                wy[:, row, None, None]
                * wx[None, :, column, None]
                * values[y[:, row, None], x[None, :, column]]
            )
    return result


def errors(actual, expected):
    rgb = np.max(
        abs(actual[:, :3] - expected[:, :3]) / np.maximum(abs(expected[:, :3]), 1e-5),
        axis=-1,
    )
    through = np.max(abs(actual[:, 3:] - expected[:, 3:]), axis=-1)
    return {
        "relative_RGB_max": float(rgb.max()),
        "relative_RGB_p99": float(np.quantile(rgb, 0.99)),
        "RGB_over_1_percent": int(np.count_nonzero(rgb > 0.01)),
        "T_abs_max": float(through.max()),
        "T_over_2e6": int(np.count_nonzero(through > 2e-6)),
        "worst_RGB_indices": np.argsort(rgb)[-8:].astype(int).tolist(),
        "worst_T_index": int(through.argmax()),
    }


def numeric(samples):
    return np.c_[samples["radiance"], samples["through"]]
