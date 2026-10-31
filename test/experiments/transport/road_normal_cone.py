"""Whole encoded analytic road normals; exact predicates, no sampled-angle admission."""

from itertools import product

import numpy as np
from road_normal_hulls import derivative_controls
from road_normal_math import (
    F,
    add,
    divide,
    exact,
    multiply,
    negative,
    polynomial_range,
    small_trig,
    sqrt_interval,
    square,
    tangent,
    trig_point,
)


def singleton(value):
    value = exact(value)
    return value, value


def limit(value):
    value = exact(value)
    if not 0 < value <= 1:
        raise ValueError(
            "Explicit positive normal angle limit at most one radian required"
        )
    return value


class NormalModel:
    def __init__(self, curve):
        if np.any(curve.width * curve.curvature_max >= 1):
            raise ValueError("Singular or folded whole analytic road sweep")
        self.curve = curve
        self.height = tuple(tuple(map(exact, row)) for row in curve.height)
        self.bank = tuple(tuple(map(exact, row)) for row in curve.bank)

    def ranges(self, index, first, last, low, high):
        curve = self.curve
        start, end = map(exact, curve.stations[index : index + 2])
        first, last, low, high = map(exact, (first, last, low, high))
        if not (
            start <= first <= last <= end and -curve.width <= low <= high <= curve.width
        ):
            raise ValueError("One bound complete source-span parameter cell required")
        length = exact(curve.lengths[index])
        a, b = (first - start) / length, (last - start) / length
        beta = polynomial_range(self.bank[index], a, b)

        grade = polynomial_range(derivative_controls(self.height[index], length), a, b)
        bank_rate = polynomial_range(
            derivative_controls(self.bank[index], length), a, b
        )
        crossfall = tangent(beta)
        rate = multiply(bank_rate, add(singleton(1), square(crossfall)))
        distance = first - start, last - start
        k, dk = exact(curve.curvature_start[index]), exact(curve.curvature_rate[index])
        curvature = add(singleton(k), multiply(singleton(dk), distance))
        jacobian = add(singleton(1), negative(multiply((low, high), curvature)))
        if jacobian[0] <= 0:
            raise ValueError("Whole source normal cell folds")
        q = divide(add(grade, negative(multiply((low, high), rate))), jacobian)

        def phase(x):
            return exact(curve.poses[index, 4]) + k * x + dk * x * x / 2

        candidates = [phase(x) for x in distance]
        if dk and distance[0] <= -k / dk <= distance[1]:
            candidates.append(phase(-k / dk))
        return q, crossfall, (min(candidates), max(candidates))

    def cone(self, index, first, last, low, high, limit_rad):
        angle = limit(limit_rad)
        q, b, phase = self.ranges(index, first, last, low, high)
        anchor_phase = sum(phase) / 2
        delta = phase[0] - anchor_phase, phase[1] - anchor_phase
        if delta[0] < -1 or delta[1] > 1:
            return {"accepted": False, "reason": "heading_range", "score": float("inf")}
        sine, cosine = small_trig(delta)
        x = add(negative(multiply(q, cosine)), negative(multiply(b, sine)))
        y = add(negative(multiply(q, sine)), multiply(b, cosine))
        anchor = (-sum(q) / 2, sum(b) / 2, F(1))
        dot_low = min(anchor[0] * v + anchor[1] * w + 1 for v, w in product(x, y))
        if dot_low <= 0:
            return {
                "accepted": False,
                "reason": "normal_hemisphere",
                "score": float("inf"),
            }
        cross2 = max(
            (anchor[1] - w) ** 2
            + (v - anchor[0]) ** 2
            + (anchor[0] * w - anchor[1] * v) ** 2
            for v, w in product(x, y)
        )
        tan_limit = tangent(singleton(angle / 4))[0]
        accepted = cross2 <= dot_low**2 * tan_limit**2
        ratio = cross2 / dot_low**2
        return {
            "accepted": accepted,
            "reason": "whole_parameter_cone",
            "score": float(ratio) if ratio < F(10) ** 300 else float("inf"),
        }

    def stored_normal(self, index, station, lateral, limit_rad):
        angle = limit(limit_rad)
        q, b, phase = self.ranges(index, station, station, lateral, lateral)
        sine, cosine = trig_point(phase[0])
        raw = (
            add(negative(multiply(q, cosine)), negative(multiply(b, sine))),
            add(negative(multiply(q, sine)), multiply(b, cosine)),
            singleton(1),
        )
        norm = sqrt_interval(add(add(square(raw[0]), square(raw[1])), square(raw[2])))
        unit = tuple(divide(v, norm) for v in raw)
        value = np.array([float(sum(v) / 2) for v in unit], dtype=np.float32)
        error2 = sum(
            max(abs(exact(x) - v[0]), abs(exact(x) - v[1])) ** 2
            for x, v in zip(value, unit, strict=True)
        )
        sine_limit = trig_point(angle / 4)[0][0]
        if error2 > sine_limit**2:
            raise ValueError(
                "Stored vertex normal exceeds its angular rounding allowance"
            )
        return value
