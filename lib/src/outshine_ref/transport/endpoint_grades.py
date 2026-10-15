"""Fixed-length C1 height profiles with explicitly prescribed contact grades."""

import math
from dataclasses import dataclass, field

import numpy as np


def derivative_bounds(breaks, coefficients):
    spans = np.diff(breaks)
    _, b, c, d = coefficients.T
    with np.errstate(over="ignore", divide="ignore", invalid="ignore"):
        roots = np.divide(-c, 3 * d, out=np.zeros(len(d)), where=d != 0)
    points = np.c_[np.zeros(len(spans)), spans, np.clip(roots, 0, spans)]
    grade = np.max(
        np.abs(b[:, None] + points * (2 * c[:, None] + 3 * d[:, None] * points))
    )
    second = np.max(np.maximum(abs(2 * c), abs(2 * c + 6 * d * spans)))
    return float(grade), float(second)


@dataclass(frozen=True, slots=True)
class EndpointTransition:
    breaks: np.ndarray
    coefficients: np.ndarray
    start_grade: float
    end_grade: float
    grade_bound: float
    second_derivative_bound: float
    _curve: object = field(init=False, repr=False)

    def __post_init__(self):
        from scipy.interpolate import PPoly

        if (
            not 2 <= len(self.breaks) <= 9
            or len(self.coefficients) != len(self.breaks) - 1
        ):
            raise ValueError("one through eight polynomial intervals required")
        if any(len(row) != 4 for row in self.coefficients):
            raise ValueError("four ascending local coefficients required")
        breaks = np.asarray(self.breaks, float)
        coefficients = np.asarray(self.coefficients, float)
        if breaks.ndim != 1 or coefficients.shape != (len(breaks) - 1, 4):
            raise ValueError(
                "one-dimensional stations and interval coefficients required"
            )
        if not np.isfinite(breaks).all() or not np.isfinite(coefficients).all():
            raise ValueError("finite bounded polynomial required")
        if breaks[0] != 0 or np.any(np.diff(breaks) <= 0) or breaks[-1] > 1e7:
            raise ValueError("bounded positive station spans starting at zero required")
        if np.abs(coefficients).max() > 1e100:
            raise ValueError("polynomial coefficient numeric domain exceeded")
        if not all(
            math.isfinite(v)
            for v in (
                self.start_grade,
                self.end_grade,
                self.grade_bound,
                self.second_derivative_bound,
            )
        ):
            raise ValueError("finite derivative requirements required")
        if min(self.grade_bound, self.second_derivative_bound) < 0:
            raise ValueError("nonnegative derivative bounds required")
        curve = PPoly(coefficients[:, ::-1].T.copy(), breaks.copy(), extrapolate=False)
        spans = np.diff(breaks)
        a, b, c, d = coefficients.T
        heights = a + spans * (b + spans * (c + spans * d))
        grades = b + spans * (2 * c + 3 * d * spans)
        if not np.allclose(heights[:-1], a[1:], rtol=0, atol=1e-7) or not np.allclose(
            grades[:-1], b[1:], rtol=0, atol=1e-10
        ):
            raise ValueError("C1 interval joins required")
        if (
            abs(b[0] - self.start_grade) > 1e-10
            or abs(grades[-1] - self.end_grade) > 1e-10
        ):
            raise ValueError("prescribed endpoint grades required")
        grade, second = derivative_bounds(breaks, coefficients)
        if (
            grade > self.grade_bound + 1e-10
            or second > self.second_derivative_bound + 1e-12
        ):
            raise ValueError("whole-interval derivative certificate exceeded")
        object.__setattr__(self, "breaks", np.frombuffer(breaks.tobytes(), dtype=float))
        object.__setattr__(
            self,
            "coefficients",
            np.frombuffer(coefficients.tobytes(), dtype=float).reshape(
                coefficients.shape
            ),
        )
        object.__setattr__(self, "_curve", curve)

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
                "height/grade/second derivative and bounded query count required"
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
            raise ValueError("finite stations inside the profile required")
        return self._curve(values, nu=derivative)


def grade_envelopes(length, start_grade, end_grade, grade, second):
    reach = second * length
    candidates = [0.0, 1.0]
    candidates.extend(
        (
            (start_grade + grade) / reach,
            1 - (end_grade + grade) / reach,
            (grade - start_grade) / reach,
            1 - (grade - end_grade) / reach,
        )
    )
    candidates.extend(
        (
            (start_grade - end_grade + reach) / (2 * reach),
            (end_grade - start_grade + reach) / (2 * reach),
        )
    )
    stations = np.unique(np.clip(candidates, 0.0, 1.0) * length)
    fractions = stations / length
    lower = np.maximum.reduce(
        (
            np.full_like(fractions, -grade),
            start_grade - reach * fractions,
            end_grade - reach * (1 - fractions),
        )
    )
    upper = np.minimum.reduce(
        (
            np.full_like(fractions, grade),
            start_grade + reach * fractions,
            end_grade + reach * (1 - fractions),
        )
    )
    areas = [
        math.fsum(np.diff(stations) * (v[:-1] + v[1:]) * 0.5) for v in (lower, upper)
    ]
    midpoints = stations[:-1] + np.diff(stations) * 0.5
    lower_slopes = np.array([0.0, -second, second])
    upper_slopes = -lower_slopes
    lower_values = (
        np.array([-grade, start_grade, end_grade - reach])[:, None]
        + lower_slopes[:, None] * midpoints
    )
    upper_values = (
        np.array([grade, start_grade, end_grade + reach])[:, None]
        + upper_slopes[:, None] * midpoints
    )
    slopes = (
        lower_slopes[np.argmax(lower_values, axis=0)],
        upper_slopes[np.argmin(upper_values, axis=0)],
    )
    return stations, lower, upper, areas, slopes


def envelope_coefficients(start_height, delta, stations, lower, upper, areas, slopes):
    span = areas[1] - areas[0]
    weight = np.clip((delta - areas[0]) / span, 0, 1) if span > 0 else 0.0
    grades = (1 - weight) * lower + weight * upper
    widths = np.diff(stations)
    acceleration = (1 - weight) * slopes[0] + weight * slopes[1]
    rises = widths * (grades[:-1] + acceleration * widths * 0.5)
    heights = np.r_[start_height, start_height + np.cumsum(rises)]
    return np.c_[heights[:-1], grades[:-1], acceleration * 0.5, np.zeros(len(widths))]


def plan_endpoint_transition(
    start_height,
    end_height,
    length,
    start_grade,
    end_grade,
    *,
    grade=0.08,
    second_derivative=0.002,
    method="envelope",
):
    values = (
        start_height,
        end_height,
        length,
        start_grade,
        end_grade,
        grade,
        second_derivative,
    )
    if not all(math.isfinite(v) for v in values):
        raise ValueError("finite metric profile inputs required")
    if max(abs(start_height), abs(end_height)) > 1e7 or not 1e-6 <= length <= 1e7:
        raise ValueError("bounded heights and positive source length required")
    if not 1e-9 <= grade <= 1 or not 1e-12 <= second_derivative <= 1:
        raise ValueError("bounded positive grade and second derivative required")
    if method not in ("envelope", "hermite"):
        raise ValueError("known endpoint profile method required")
    report = {
        "valid": False,
        "reason": "endpoint_grade_out_of_bounds",
        "method": method,
        "length_m": length,
    }
    if max(abs(start_grade), abs(end_grade)) > grade:
        return None, report
    if abs(end_grade - start_grade) > second_derivative * length + 1e-12:
        report["reason"] = "endpoint_grade_rate_infeasible"
        return None, report
    delta = end_height - start_height
    if method == "envelope":
        stations, lower, upper, areas, slopes = grade_envelopes(
            length, start_grade, end_grade, grade, second_derivative
        )
        report["reachable_height_change_m"] = areas
        tolerance = (
            64
            * np.finfo(float).eps
            * max(1, abs(start_height), abs(end_height), length * grade)
        )
        if delta < areas[0] - tolerance or delta > areas[1] + tolerance:
            report["reason"] = "height_outside_reachable_grade_envelope"
            return None, report
        breaks = stations
        coefficients = envelope_coefficients(
            start_height, delta, stations, lower, upper, areas, slopes
        )
    else:
        breaks = [0.0, length]
        mean = delta / length
        coefficients = np.array(
            [
                [
                    start_height,
                    start_grade,
                    (3 * mean - 2 * start_grade - end_grade) / length,
                    (start_grade + end_grade - 2 * mean) / length**2,
                ]
            ]
        )
    actual_grade, actual_second = derivative_bounds(np.asarray(breaks), coefficients)
    if actual_grade > grade + 1e-10 or actual_second > second_derivative + 1e-12:
        report["reason"] = "method_derivative_bound_exceeded"
        return None, report
    try:
        profile = EndpointTransition(
            breaks, coefficients, start_grade, end_grade, grade, second_derivative
        )
    except ValueError as failure:
        report["reason"] = "numerically_infeasible_profile: " + str(failure)
        return None, report
    endpoint_error = abs(float(profile.evaluate([length])[0]) - end_height)
    if endpoint_error > 1e-7:
        report["reason"] = "profile_endpoint_height_error"
        return None, report
    report.update(
        valid=True,
        reason="",
        maximum_grade=actual_grade,
        maximum_second_derivative_per_m=actual_second,
        endpoint_height_error_m=endpoint_error,
        intervals=len(breaks) - 1,
        c1_prescribed_endpoint_grades=True,
        c2_joins=False,
    )
    return profile, report
