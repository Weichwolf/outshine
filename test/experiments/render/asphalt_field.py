"""Dry asphalt aggregate reference: zero-integral Gaussian grain/binder kernels."""

import itertools

import numpy as np

SIGMA = 0.12
ALBEDO = np.array([0.11, 0.11, 0.10])
ALBEDO_AMPLITUDE = 0.035
ROUGHNESS = 0.92
ROUGHNESS_AMPLITUDE = 0.02


def cell_hash(cell):
    p = np.asarray(cell, dtype=np.int64).astype(np.uint32)
    h = p[..., 0] * np.uint32(0x9E3779B9) ^ p[..., 1] * np.uint32(0x85EBCA6B)
    h ^= h >> np.uint32(16)
    h *= np.uint32(0x7FEB352D)
    h ^= h >> np.uint32(15)
    h *= np.uint32(0x846CA68B)
    return h ^ (h >> np.uint32(16))


def grain(cell):
    bits = cell_hash(cell)
    x = (bits & 1023).astype(float) / 1023
    y = ((bits >> 10) & 1023).astype(float) / 1023
    weight = 0.75 + 0.5 * ((bits >> 20) & 1023).astype(float) / 1023
    return np.asarray(cell) + 0.2 + 0.6 * np.stack([x, y], axis=-1), weight


def point_field(points, radius=3):
    """Reference sums 7x7 cells; wider 9x9 is a separate tail control."""
    points = np.asarray(points)
    cells = np.floor(points).astype(np.int64)
    field = np.zeros(points.shape[:-1])
    for offset in itertools.product(range(-radius, radius + 1), repeat=2):
        site, weight = grain(cells + offset)
        square = np.sum((points - site) ** 2, axis=-1)
        field += weight * (
            np.exp(-square / (2 * SIGMA**2)) - 0.25 * np.exp(-square / (8 * SIGMA**2))
        )
    return field


def covariance_response(points, dx, dy, isotropic=False):
    """Analytic Gaussian convolution; box-pixel Gaussian approximation is tested."""
    points = np.asarray(points)
    cells = np.floor(points).astype(np.int64)
    covariance = (np.outer(dx, dx) + np.outer(dy, dy)) / 12
    if isotropic:
        covariance = np.eye(2) * max(np.dot(dx, dx), np.dot(dy, dy)) / 12
    matrices = [np.eye(2) * s * s + covariance for s in (SIGMA, 2 * SIGMA)]
    inverses = [np.linalg.inv(m) for m in matrices]
    amplitudes = [
        s * s / np.sqrt(np.linalg.det(m))
        for s, m in zip((SIGMA, 2 * SIGMA), matrices, strict=True)
    ]
    field = np.zeros(points.shape[:-1])
    for offset in itertools.product(range(-1, 2), repeat=2):
        site, weight = grain(cells + offset)
        delta = points - site
        kernels = [
            a * np.exp(-0.5 * np.einsum("...i,ij,...j->...", delta, inverse, delta))
            for a, inverse in zip(amplitudes, inverses, strict=True)
        ]
        field += weight * (kernels[0] - 0.25 * kernels[1])
    return field


def filtered(points, dx, dy, method):
    width = max(np.linalg.norm(dx), np.linalg.norm(dy))
    if method.endswith("_minor") or method == "oriented":
        # Both footprint axes must be unresolved before discarding the field.
        trace = np.dot(dx, dx) + np.dot(dy, dy)
        determinant = (dx[0] * dy[1] - dx[1] * dy[0]) ** 2
        divisor = trace + np.sqrt(max(trace * trace - 4 * determinant, 0))
        width = np.sqrt(2 * determinant / divisor) if divisor > 0 else 0
        method = method.removesuffix("_minor")
    fade = np.clip((width - 2) / 2, 0, 1)
    visibility = 1 - fade * fade * (3 - 2 * fade)
    if visibility == 0:
        return np.zeros(np.asarray(points).shape[:-1])
    if method == "point":
        fade = np.clip((width - 0.25) / 0.75, 0, 1)
        return point_field(points, radius=1) * (1 - fade * fade * (3 - 2 * fade))
    if method in ("isotropic", "jacobian"):
        return covariance_response(points, dx, dy, method == "isotropic") * visibility
    if method not in ("four", "oriented"):
        raise ValueError("unknown asphalt filter")
    result = np.zeros(np.asarray(points).shape[:-1])
    nx, ny = 2, 2
    if method == "oriented":
        if np.linalg.norm(dx) > 2 * np.linalg.norm(dy):
            nx, ny = 4, 1
        elif np.linalg.norm(dy) > 2 * np.linalg.norm(dx):
            nx, ny = 1, 4
    for u, v in itertools.product(
        (np.arange(nx) + 0.5) / nx - 0.5, (np.arange(ny) + 0.5) / ny - 0.5
    ):
        result += covariance_response(points + u * dx + v * dy, dx / nx, dy / ny)
    return result * visibility / 4


def parameters(field, albedo=ALBEDO, roughness=ROUGHNESS):
    return albedo + ALBEDO_AMPLITUDE * field[
        ..., None
    ], roughness + ROUGHNESS_AMPLITUDE * field


def pixel_reference(points, dx, dy, order):
    nodes, weights = np.polynomial.legendre.leggauss(order)
    offsets = np.array(list(itertools.product(nodes / 2, repeat=2)))
    weights = np.outer(weights / 2, weights / 2).ravel()
    result = np.zeros(len(points))
    # Bound scratch memory independently of the number of reference pixels.
    for start in range(0, len(points), 16):
        sample = (
            points[start : start + 16, None] + offsets[:, :1] * dx + offsets[:, 1:] * dy
        )
        result[start : start + 16] = point_field(sample) @ weights
    return result
