"""Independent homogeneous-air controls for spatial source partitions (#320).

The cloud is a prescribed absorbing curtain above a horizontal camera ray.
Solar-LUT and diffuse/Psi inputs are fixed; no global LUT convergence is claimed.
"""

from dataclasses import asdict, dataclass
from functools import cache
from itertools import pairwise

import numpy as np

SIGMA = np.array([0.14, 0.24, 0.34])
RAYLEIGH = np.array([0.1, 0.2, 0.3])
MIE, G = 0.03, 0.8
PHASE_R = 3 / (16 * np.pi)
PHASE_M = 3 / (8 * np.pi) * (1 - G * G) / (2 + G * G) / (1 + G * G) ** 1.5
DIRECT = np.array([0.75, 0.5, 0.25]) * (RAYLEIGH * PHASE_R + MIE * PHASE_M)
DIFFUSE = (RAYLEIGH + MIE) * np.array([0.015625, 0.03125, 0.0625])
RADIANCE_FLOOR = 1e-5
RADIANCE_LIMIT = 0.01


@dataclass(frozen=True)
class ShadowControl:
    center_km: float
    width_km: float
    cloud_optical_depth: float
    query_end_km: float


def controls(extent_km):
    return tuple(
        ShadowControl(
            center, width, depth, min(extent_km, max(1.2, center + 4 * width))
        )
        for center in (0.15, 0.45, 1.0, 3.0, 10.0, 25.0)
        if center < extent_km
        for width in (0.05, 0.2, 0.8)
        for depth in (0.25, 2.0, 8.0)
    )


@cache
def quadrature(count):
    nodes, weights = np.polynomial.legendre.leggauss(count)
    nodes.flags.writeable = weights.flags.writeable = False
    return nodes, weights


def source(distance_km, control):
    distance = np.asarray(distance_km)
    optical = control.cloud_optical_depth * np.exp(
        -(((distance - control.center_km) / control.width_km) ** 2)
    )
    return np.exp(-optical)[..., None] * DIRECT + DIFFUSE


def reference(queries_km, control, count=128):
    """Closed-form clear source minus independently integrated shadow deficit."""
    queries = np.asarray(queries_km, dtype=float)
    nodes, weights = quadrature(count)
    result = -np.expm1(-queries[:, None] * SIGMA) / SIGMA * (DIRECT + DIFFUSE)
    cuts = np.unique(
        np.maximum(
            0,
            control.center_km
            + control.width_km * np.array([-8, -4, -2, -1, 0, 1, 2, 4, 8]),
        )
    )
    if cuts[0] > 0:
        cuts = np.insert(cuts, 0, 0)
    for first, last in pairwise(cuts):
        reach = np.clip(queries - first, 0, last - first)
        positions = first + reach[:, None] * (nodes + 1) / 2
        deficit = -np.expm1(
            -control.cloud_optical_depth
            * np.exp(-(((positions - control.center_km) / control.width_km) ** 2))
        )
        result -= (
            reach[:, None]
            / 2
            * np.einsum(
                "n,qn,qnc->qc",
                weights,
                deficit,
                np.exp(-positions[:, :, None] * SIGMA) * DIRECT,
            )
        )
    return result


def grid_edges(extent_km, kind, count=22, scale_km=0.1):
    fraction = np.linspace(0, 1, count + 1)
    if kind == "quadratic":
        return extent_km * fraction**2
    if kind == "cubic":
        return extent_km * fraction**3
    if kind == "quartic":
        return extent_km * fraction**4
    if kind == "log":
        result = scale_km * np.expm1(np.log1p(extent_km / scale_km) * fraction)
        result[-1] = extent_km
        return result
    if kind == "metric":
        return np.minimum(
            np.arange(int(np.ceil(extent_km / scale_km)) + 1) * scale_km,
            extent_km,
        )
    raise ValueError(f"unknown source grid: {kind}")


def fitted_transfer(queries_km, edges, control):
    """Isolate positive Bernstein fitting, using refined cell integration."""
    queries = np.asarray(queries_km, dtype=float)
    nodes, weights = quadrature(24)
    result = np.zeros((len(queries), 3))
    for first, last in pairwise(edges):
        if first >= queries.max():
            break
        a, middle, c = source(np.array([first, (first + last) / 2, last]), control)
        b = np.maximum(0, 2 * middle - (a + c) / 2)
        reach = np.clip(queries - first, 0, last - first)
        local = reach[:, None] * (nodes + 1) / 2
        fraction = local[:, :, None] / (last - first)
        light = (
            a * (1 - fraction) ** 2
            + 2 * b * fraction * (1 - fraction)
            + c * fraction**2
        )
        result += (
            reach[:, None]
            / 2
            * np.einsum(
                "n,qnc->qc",
                weights,
                np.exp(-(local + first)[:, :, None] * SIGMA) * light,
            )
        )
    return result


def relative_errors(actual, expected):
    return np.max(
        np.abs(actual - expected) / np.maximum(expected, RADIANCE_FLOOR), axis=1
    )


def queries(control):
    return np.repeat(np.linspace(0, control.query_end_km, 129, dtype="<f4"), 2)


def describe(control):
    return asdict(control)
