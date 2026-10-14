"""Flat-ended C1 elevation plans with whole-interval grade/second-derivative bounds."""

import math
from dataclasses import dataclass, field

import numpy as np


@dataclass(frozen=True, slots=True)
class VerticalTransition:
    breaks: np.ndarray
    coefficients: np.ndarray
    grade_bound: float
    second_derivative_bound: float
    _curve: object = field(init=False, repr=False)

    def __post_init__(self):
        from scipy.interpolate import PPoly

        if not 2 <= len(self.breaks) <= 4 or not 1 <= len(self.coefficients) <= 3:
            raise ValueError(
                "at most three intervals required before numeric conversion"
            )
        if any(len(row) != 4 for row in self.coefficients):
            raise ValueError("four coefficients required before numeric conversion")
        breaks, coefficients = (
            np.array(self.breaks, float),
            np.array(self.coefficients, float),
        )
        if (
            breaks.ndim != 1
            or not 2 <= len(breaks) <= 4
            or coefficients.shape != (len(breaks) - 1, 4)
        ):
            raise ValueError("one to three ascending polynomial intervals required")
        if (
            not np.isfinite(breaks).all()
            or not np.isfinite(coefficients).all()
            or breaks[0] != 0
            or np.any(np.diff(breaks) <= 0)
        ):
            raise ValueError(
                "finite increasing station intervals and coefficients required"
            )
        if not all(
            math.isfinite(v) and v >= 0
            for v in (self.grade_bound, self.second_derivative_bound)
        ):
            raise ValueError("finite nonnegative derivative certificates required")
        if breaks[-1] > 1e7 or np.abs(coefficients).max() > 1e100:
            raise ValueError(
                "station span or polynomial coefficient exceeds numeric domain"
            )
        spans = np.diff(breaks)
        a, b, c, d = coefficients.T
        ends = a + spans * (b + spans * (c + spans * d))
        grades = b + spans * (2 * c + 3 * d * spans)
        if (
            abs(b[0]) > 1e-10
            or abs(grades[-1]) > 1e-10
            or not np.allclose(ends[:-1], a[1:], rtol=0, atol=1e-7)
            or not np.allclose(grades[:-1], b[1:], rtol=0, atol=1e-10)
        ):
            raise ValueError("C1 profile with flat end contacts required")
        with np.errstate(over="ignore", divide="ignore", invalid="ignore"):
            roots = np.divide(-c, 3 * d, out=np.zeros(len(d)), where=d != 0)
        extrema = np.c_[
            np.zeros(len(spans)),
            spans,
            np.clip(roots, 0, spans),
        ]
        peak_grade = abs(
            b[:, None] + extrema * (2 * c[:, None] + 3 * d[:, None] * extrema)
        ).max()
        peak_second = np.maximum(abs(2 * c), abs(2 * c + 6 * d * spans)).max()
        if (
            peak_grade > self.grade_bound + 1e-10
            or peak_second > self.second_derivative_bound + 1e-12
        ):
            raise ValueError(
                "polynomial exceeds its whole-interval derivative certificate"
            )
        breaks = np.frombuffer(breaks.tobytes(), dtype=float)
        coefficients = np.frombuffer(coefficients.tobytes(), dtype=float).reshape(
            coefficients.shape
        )
        object.__setattr__(self, "breaks", breaks)
        object.__setattr__(self, "coefficients", coefficients)
        object.__setattr__(
            self,
            "_curve",
            PPoly(coefficients[:, ::-1].T.copy(), breaks.copy(), extrapolate=False),
        )

    @property
    def length_m(self):
        return float(self.breaks[-1])

    def evaluate(self, stations, derivative=0, *, max_queries=65536):
        if (
            derivative not in (0, 1, 2)
            or type(max_queries) is not int
            or max_queries < 0
        ):
            raise ValueError(
                "height/grade/second derivative and nonnegative query cap required"
            )
        count = stations.size if isinstance(stations, np.ndarray) else len(stations)
        if count > max_queries:
            raise ValueError("station query cap exceeded")
        values = np.asarray(stations, float)
        if (
            values.ndim != 1
            or not np.isfinite(values).all()
            or np.any(values < 0)
            or np.any(values > self.length_m)
        ):
            raise ValueError(
                "finite one-dimensional stations within the profile required"
            )
        return self._curve(values, nu=derivative)


def minimum_transition_length(
    height_change, grade, second_derivative, *, method="parabolic"
):
    h = abs(float(height_change))
    if (
        not 0 <= h <= 2e7
        or not 1e-9 <= grade <= 1
        or not 1e-12 <= second_derivative <= 1
    ):
        raise ValueError(
            "height change <=2e7 m, grade1e-9..1 and second derivative1e-12..1/m required"
        )
    if method == "parabolic":
        return (
            2 * math.sqrt(h / second_derivative)
            if h <= grade * grade / second_derivative
            else h / grade + grade / second_derivative
        )
    if method == "cubic":
        return max(1.5 * h / grade, math.sqrt(6 * h / second_derivative))
    raise ValueError("known flat-ended profile method required")


def plan_vertical_transition(
    start_height,
    end_height,
    length,
    *,
    grade=0.08,
    second_derivative=0.002,
    method="parabolic",
):
    """Return a profile or a local infeasibility reason; coordinates are metres.

    For C1 flat-ended profiles, |z'|<=g and |z''|<=a almost everywhere imply
    |z'|<=min(a*s,g,a*(L-s)). Its integral bounds the possible height change.
    The parabolic/linear/parabolic plan attains that bound at minimum length.
    At a fixed feasible length it minimizes the peak second derivative: solve
    the same envelope area for its smallest feasible bound. This is only a
    one-dimensional flat-ended result, not a network optimum.
    """
    start_height, end_height, length = map(float, (start_height, end_height, length))
    if (
        not all(math.isfinite(v) and abs(v) <= 1e7 for v in (start_height, end_height))
        or not 1e-6 <= length <= 1e7
    ):
        raise ValueError(
            "finite heights within +/-1e7 m and length1e-6..1e7 m required"
        )
    delta = end_height - start_height
    required = minimum_transition_length(delta, grade, second_derivative, method=method)
    report = {
        "method": method,
        "length_m": length,
        "minimum_transition_length_m": required,
        "valid": False,
        "reason": "insufficient_profile_length",
    }
    if length < required * (1 - 1e-12):
        return None, report
    if delta == 0:
        breaks, coefficients = [0.0, length], [[start_height, 0.0, 0.0, 0.0]]
        actual_grade, actual_second = 0.0, 0.0
    elif method == "cubic":
        breaks, coefficients = (
            [0.0, length],
            [[start_height, 0.0, 3 * delta / length**2, -2 * delta / length**3]],
        )
        actual_grade, actual_second = (
            1.5 * abs(delta) / length,
            6 * abs(delta) / length**2,
        )
    else:
        h, direction = abs(delta), math.copysign(1.0, delta)
        peak = min(2 * h / length, grade)
        actual_second = (
            4 * h / length**2 if peak < grade else grade**2 / (grade * length - h)
        )
        ramp = peak / actual_second
        if length - 2 * ramp <= length * 1e-12:
            breaks = [0.0, length / 2, length]
            coefficients = [
                [start_height, 0.0, direction * actual_second / 2, 0.0],
                [
                    start_height + delta / 2,
                    direction * peak,
                    -direction * actual_second / 2,
                    0.0,
                ],
            ]
        else:
            breaks = [0.0, ramp, length - ramp, length]
            corner = peak * ramp / 2
            coefficients = [
                [start_height, 0.0, direction * actual_second / 2, 0.0],
                [start_height + direction * corner, direction * peak, 0.0, 0.0],
                [
                    end_height - direction * corner,
                    direction * peak,
                    -direction * actual_second / 2,
                    0.0,
                ],
            ]
        actual_grade = peak
    if actual_grade > grade * (1 + 1e-10) or actual_second > second_derivative * (
        1 + 1e-10
    ):
        report["reason"] = "numerically_infeasible_profile"
        return None, report
    profile = VerticalTransition(breaks, coefficients, actual_grade, actual_second)
    report.update(
        valid=True,
        reason="",
        maximum_grade=actual_grade,
        maximum_second_derivative_per_m=actual_second,
        intervals=len(breaks) - 1,
        c1_flat_endpoints=True,
        c2_at_flat_contacts=delta == 0,
    )
    return profile, report


def sample_vertical_transition(profile, *, height_error_m=0.01, max_vertices=4096):
    if (
        not math.isfinite(height_error_m)
        or height_error_m <= 0
        or type(max_vertices) is not int
        or max_vertices < 2
    ):
        raise ValueError(
            "positive height error and at least two allowed vertices required"
        )
    spans = np.diff(profile.breaks)
    c, d = profile.coefficients[:, 2], profile.coefficients[:, 3]
    curvature = np.maximum(abs(2 * c), abs(2 * c + 6 * d * spans))
    pieces = [
        max(1, math.ceil(length * math.sqrt(bound) / math.sqrt(8 * height_error_m)))
        for length, bound in zip(spans, curvature)
    ]
    report = {
        "reason": "profile_vertex_budget_exhausted",
        "required_vertices": 1 + sum(pieces),
        "height_error_bound_m": height_error_m,
    }
    if report["required_vertices"] > max_vertices:
        return None, report
    stations = np.r_[
        0.0,
        np.concatenate(
            [
                np.linspace(a, b, count + 1)[1:]
                for a, b, count in zip(profile.breaks[:-1], profile.breaks[1:], pieces)
            ]
        ),
    ]
    bounds = curvature * (spans / pieces) ** 2 / 8
    report.update(
        reason="",
        maximum_height_chord_bound_m=float(bounds.max()),
        vertices=len(stations),
    )
    return np.c_[stations, profile.evaluate(stations, max_queries=max_vertices)], report
