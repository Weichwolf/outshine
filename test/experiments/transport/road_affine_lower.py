"""A bounded affine lower approximation on straight source crossfall ramps."""

import itertools
import math

import numpy as np
from road_normal_hulls import derivative_controls, split_half, upward


def interval(values):
    return -upward(-min(values)), upward(max(values))


def source_hulls(curve):
    heights, rates = [], []
    for height, bank, length in zip(
        curve.height, curve.bank, curve.lengths, strict=True
    ):
        heights.extend(
            value
            for half in split_half(derivative_controls(height, length))
            for value in half
        )
        rates.extend(
            value
            for half in split_half(derivative_controls(bank, length))
            for value in half
        )
    h, db = interval(heights), interval(rates)
    bank = (float(curve.bank.min()), float(curve.bank.max()))
    q = (
        math.nextafter(math.tan(bank[0]), -math.inf),
        math.nextafter(math.tan(bank[1]), math.inf),
    )
    smallest = 0 if q[0] <= 0 <= q[1] else min(map(abs, q))
    largest = max(map(abs, q))
    sec = (
        math.nextafter(1 + smallest**2, -math.inf),
        math.nextafter(1 + largest**2, math.inf),
    )
    products = [a * b for a in db for b in sec]
    dq = (
        math.nextafter(min(products), -math.inf),
        math.nextafter(max(products), math.inf),
    )
    return h, q, dq


def box_distance(p, q, along, drop, thickness):
    """Exact real-arithmetic box maximum: p stationary points minimize distance.

    For positive drop, dot(N,w)'s sole p stationary point is a maximum.
    Extremal absolute q and convexity in along then reduce the box to corners.
    Floating evaluation remains subject to the existing libm/model limitation.
    """
    q_abs = (0 if q[0] <= 0 <= q[1] else min(map(abs, q)), max(map(abs, q)))
    maximum = 0.0
    for x, y, target in itertools.product(p, q_abs, along):
        den = math.hypot(1, x, y)
        distance = math.hypot(
            thickness * (-x / den) - target,
            thickness * (y / den),
            thickness / den - drop,
        )
        maximum = max(maximum, distance)
    return math.nextafter(maximum, math.inf)


def normal_departure(curve, h, q, dq, beta, bias, drop, centre, boxes):
    grid = np.linspace(-curve.width, curve.width, boxes + 1)
    maximum = 0.0
    for lo, hi in itertools.pairwise(grid):
        products = [u * rate for u in (lo, hi) for rate in dq]
        p = (
            math.nextafter(h[0] - max(products), -math.inf),
            math.nextafter(h[1] - min(products), math.inf),
        )
        shifts = [beta * (u + origin) + bias for u in (lo, hi) for origin in centre]
        along = (
            math.nextafter(min(shifts), -math.inf),
            math.nextafter(max(shifts), math.inf),
        )
        maximum = max(maximum, box_distance(p, q, along, drop, curve.thickness))
    cap_shifts = [
        beta * (u + origin) + bias
        for u in (-curve.width, curve.width)
        for origin in centre
    ]
    along = (
        math.nextafter(min(cap_shifts), -math.inf),
        math.nextafter(max(cap_shifts), math.inf),
    )
    for i, end in ((0, 0), (-1, -1)):
        p = interval([derivative_controls(curve.height[i], curve.lengths[i])[end]])
        bank = curve.bank[i, 0 if end == 0 else -1]
        cap_q = (
            math.nextafter(math.tan(bank), -math.inf),
            math.nextafter(math.tan(bank), math.inf),
        )
        maximum = max(maximum, box_distance(p, cap_q, along, drop, curve.thickness))
    return maximum


def plan_lower(curve, *, boxes=16):
    if type(boxes) is not int or boxes not in (8, 16, 32, 64):
        raise ValueError("Bounded affine width boxes8/16/32/64 required")
    if (
        np.any(curve.curvature_max != 0)
        or np.any(curve.poses[:, 4] != curve.poses[0, 4])
        or np.any(curve.middle[:, 4] != curve.poses[0, 4])
    ):
        raise ValueError("Affine lower requires one straight source frame")
    if (
        derivative_controls(curve.bank[0], curve.lengths[0])[0] != 0
        or derivative_controls(curve.bank[-1], curve.lengths[-1])[-1] != 0
    ):
        raise ValueError("Affine lower requires section-constant original cap normals")
    if np.max(abs(curve.bank)) >= math.pi / 2:
        raise ValueError("Affine crossfall hull unavailable")
    h, q, dq = source_hulls(curve)
    radius = max(map(abs, h)) + curve.width * max(map(abs, dq))
    den = math.hypot(1, radius, max(map(abs, q)))
    beta = curve.thickness * (dq[0] / 2 + dq[1] / 2) / den
    bias = -curve.thickness * (h[0] / 2 + h[1] / 2) / 2
    drop = curve.thickness * (1 + 1 / den) / 2
    theta = curve.poses[0, 4]
    tangent = np.array([math.cos(theta), math.sin(theta)])
    left = np.array([-tangent[1], tangent[0]])
    origin = curve.poses[0, 1:3].copy()
    centres = (np.vstack((curve.poses[:, 1:3], curve.middle[:, 1:3])) - origin) @ left
    centre = (
        math.nextafter(float(centres.min()), -math.inf),
        math.nextafter(float(centres.max()), math.inf),
    )
    departure = normal_departure(curve, h, q, dq, beta, bias, drop, centre, boxes)
    magnitude = (
        max(
            float(abs(curve.poses[:, 1:3]).max())
            + float(curve.lengths.sum())
            + curve.width
            + abs(beta) * (curve.width + max(map(abs, centre)))
            + abs(bias),
            float(abs(curve.height).max()) + curve.width * max(map(abs, q)) + drop,
        )
        + curve.evaluation_error_bound_m
    )
    if not math.isfinite(magnitude) or magnitude >= float(np.finfo(np.float32).max) / 2:
        raise ValueError("Affine lower Float32 magnitude unavailable")
    rounding = math.nextafter(
        math.sqrt(3)
        * float(np.spacing(np.nextafter(np.float32(magnitude), np.float32(math.inf)))),
        math.inf,
    )
    stretch = math.nextafter(math.hypot(1, beta / 2) + abs(beta) / 2, math.inf)
    if not math.isfinite(departure) or rounding >= drop:
        raise ValueError("Affine lower positive stored depth unavailable")
    return {
        "beta": beta,
        "bias": bias,
        "drop": drop,
        "tangent": tangent,
        "left": left,
        "origin": origin,
        "departure": departure,
        "stretch": stretch,
        "rounding": rounding,
        "boxes": boxes,
    }


def lower_positions(top, plan):
    exact = top.astype(float)
    across = (exact[:, :2] - plan["origin"]) @ plan["left"]
    exact[:, :2] -= (plan["beta"] * across + plan["bias"])[:, None] * plan["tangent"]
    exact[:, 2] -= plan["drop"]
    lower = exact.astype(np.float32)
    error = float(np.linalg.norm(lower.astype(float) - exact, axis=1).max())
    return lower, error
