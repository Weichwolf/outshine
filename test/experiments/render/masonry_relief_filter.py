"""Bounded analytic box filters for a declared two-millimetre brick bevel."""

import itertools

import numpy as np
from ground_gradients import surface_gradient
from masonry_filter import BAND_DARKENING, JOINT_M, MODULE_M, band

DEPTH_M = 0.002
BEVEL_M = 0.002


def ramp_box(centre, span, width):
    """Mean of clamp(x/width,0,1) and its derivative on one interval."""
    left, right = centre - span / 2, centre + span / 2
    begin, end = np.maximum(left, 0), np.minimum(right, width)
    length = np.maximum(end - begin, 0)
    area = length * (begin + end) / (2 * width)
    area += np.maximum(right - np.maximum(left, width), 0)
    safe = np.maximum(span, 1e-12)
    inside = (left >= 0) & (right <= width)
    value = np.where(inside, centre / width, area / safe)
    value = np.where(right <= 0, 0, np.where(left >= width, 1, value))
    slope = np.where(inside, 1 / width, length / (safe * width))
    point = span == 0
    value = np.where(point, np.clip(centre / width, 0, 1), value)
    slope = np.where(point, ((centre >= 0) & (centre < width)) / width, slope)
    return value, slope


def segment(centre, span, low, high, bevel):
    rise, rise_slope = ramp_box(centre - low + bevel / 2, span, bevel)
    fall, fall_slope = ramp_box(centre - high + bevel / 2, span, bevel)
    return rise - fall, rise_slope - fall_slope


def bevel_box(centre, span, period, low, high, bevel=BEVEL_M):
    """Periodic piecewise-linear profile; no quadrature or noise evaluations."""
    phase = np.remainder(centre, period)
    tail = np.remainder(span, period)
    left = np.remainder(phase - span / 2, period)
    right = left + tail
    lengths = (np.minimum(right, period) - left, np.maximum(right - period, 0))
    first = segment(left + lengths[0] / 2, lengths[0], low, high, bevel)
    second = segment(lengths[1] / 2, lengths[1], low, high, bevel)
    safe = np.maximum(span, 1e-12)
    average = (high - low) / period
    value = (
        average * (span - tail) + first[0] * lengths[0] + second[0] * lengths[1]
    ) / safe
    slope = (first[1] * lengths[0] + second[1] * lengths[1]) / safe
    plain = segment(phase, span, low, high, bevel)
    no_wrap = (span < period) & (phase - span / 2 >= 0) & (phase + span / 2 <= period)
    value = np.where(no_wrap, plain[0], value)
    slope = np.where(no_wrap, plain[1], slope)
    return np.clip(value, 0, 1), slope


def brick_gradient(at, width, depth=DEPTH_M):
    gradient = np.zeros_like(at)
    for parity in (0, 1):
        horizontal = bevel_box(
            at[..., 0] - parity * MODULE_M[0] / 2,
            width[..., 0],
            MODULE_M[0],
            JOINT_M / 2,
            MODULE_M[0] - JOINT_M / 2,
        )
        vertical = bevel_box(
            at[..., 1],
            width[..., 1],
            2 * MODULE_M[1],
            parity * MODULE_M[1] + JOINT_M / 2,
            (parity + 1) * MODULE_M[1] - JOINT_M / 2,
        )
        gradient[..., 0] += depth * horizontal[1] * vertical[0]
        gradient[..., 1] += depth * horizontal[0] * vertical[1]
    return gradient


def coverage(at, width, low, high):
    width = np.maximum(width, 0.0001)
    phase = np.remainder(at - (low + high) / 2 + 0.5, 1)
    inside = (phase - width / 2 >= (1 - high + low) / 2) & (
        phase + width / 2 <= (1 + high - low) / 2
    )
    return np.where(inside & (high > low), 1, band(at, width, 1, low, high))


def wall_weight(at, width, fractions, per_m):
    opening = np.ones(at.shape[:-1])
    for axis in (0, 1):
        opening *= coverage(
            at[..., axis],
            width[..., axis],
            (1 - fractions[..., axis]) / 2,
            (1 + fractions[..., axis]) / 2,
        )
    opening = np.where(np.min(fractions, axis=-1) > 0, opening, 0)
    stripe = np.minimum(0.16 * np.maximum(per_m, 0), 1 - fractions[..., 1])
    courses = coverage(at[..., 1], width[..., 1], -stripe / 2, stripe / 2)
    return np.maximum(1 - opening - BAND_DARKENING * courses, 0)


def filtered(at, dx, dy, fractions, per_m, side, depth=DEPTH_M):
    point = side == 0
    side = max(side, 1)
    width = (np.abs(dx) + np.abs(dy)) / side
    if point:
        width = np.zeros_like(width)
    gradient = np.zeros((len(at), 2))
    nodes = (np.arange(side) + 0.5) / side - 0.5
    for x, y in itertools.product(nodes, repeat=2):
        position = at + x * dx + y * dy
        wall = wall_weight(position[:, :2], width[:, :2], fractions, per_m)
        gradient += wall[:, None] * brick_gradient(position[:, 2:], width[:, 2:], depth)
    return gradient / (side * side)


def normal(gradient, dx=None, dy=None, authored=None):
    if dx is None:
        value = np.column_stack([-gradient, np.ones(len(gradient))])
    else:
        base = np.array([0.0, 0.0, 1.0])
        projected = [
            surface_gradient(base, np.r_[u, 0], np.r_[v, 0], g @ u, g @ v, True)
            for g, u, v in zip(gradient, dx, dy, strict=True)
        ]
        value = base - np.array(projected)
    if authored is not None:
        value -= np.r_[authored, 0]
    return value / np.linalg.norm(value, axis=-1)[:, None]
