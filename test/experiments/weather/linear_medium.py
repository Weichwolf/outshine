"""Analytic transfer for a fixed union of constant-extinction linear sources."""

from math import factorial

import numpy as np


def cell(extinction, source_a, source_b, width, distance):
    sigma = np.asarray(extinction, float)
    width = np.asarray(width, float)[..., None]
    distance = np.asarray(distance, float)[..., None]
    if np.any(sigma < 0) or np.any(width <= 0):
        raise ValueError("extinction must be nonnegative and cell width positive")
    if np.any(distance < 0) or np.any(distance > width):
        raise ValueError("partial query must remain inside its fixed cell")
    tau = sigma * distance
    integral = np.divide(
        -np.expm1(-tau),
        sigma,
        out=np.broadcast_to(distance, tau.shape).copy(),
        where=sigma != 0,
    )
    thin = tau < 0.25
    series = np.zeros_like(tau)
    for order in range(8, -1, -1):
        series = series * tau + (-1) ** order / (factorial(order) * (order + 2))
    moment = np.divide(
        integral - distance * np.exp(-tau), sigma, out=np.zeros_like(tau), where=~thin
    )
    moment = np.where(thin, distance * distance * series, moment)
    weight_b = moment / width
    radiance = (
        np.asarray(source_a) * (integral - weight_b) + np.asarray(source_b) * weight_b
    )
    return radiance, np.exp(-tau)


def prefix(edges, extinction, source_a, source_b, queries):
    widths = np.diff(edges)
    queries = np.asarray(queries, float)
    if edges[0] != 0 or np.any(widths <= 0):
        raise ValueError("fixed cell edges must increase strictly from zero")
    if np.any(queries < 0) or np.any(queries > edges[-1]):
        raise ValueError("query is outside the physical prefix")
    full, _ = cell(extinction, source_a, source_b, widths, widths)
    tau = np.r_[np.zeros((1, 3)), np.cumsum(extinction * widths[:, None], axis=0)]
    radiance = np.r_[np.zeros((1, 3)), np.cumsum(np.exp(-tau[:-1]) * full, axis=0)]
    index = np.minimum(
        np.searchsorted(edges, queries, side="right") - 1, len(widths) - 1
    )
    partial, _ = cell(
        extinction[index],
        source_a[index],
        source_b[index],
        widths[index],
        queries - edges[index],
    )
    total = radiance[index] + np.exp(-tau[index]) * partial
    through = np.exp(
        -tau[index] - extinction[index] * (queries - edges[index])[:, None]
    )
    return np.c_[total, through]
