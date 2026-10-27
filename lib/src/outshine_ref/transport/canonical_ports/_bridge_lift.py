"""Source-fixed coarse DEM obstacles and a bounded endpoint-C1 quartic profile lift."""

import math
from fractions import Fraction

import numpy as np

from ._geodesy import ground_geographic_at, positions_in_frame


class LiftedProfile:
    def __init__(self, profile, amplitude):
        self.base = profile
        self.amplitude = amplitude
        self.length_m = profile.length_m

    def evaluate(self, stations, derivative=0):
        s = np.asarray(stations, float)
        u = s / self.length_m
        if derivative == 0:
            q = 16 * u * u * (1 - u) ** 2
        elif derivative == 1:
            q = 32 * u * (1 - u) * (1 - 2 * u) / self.length_m
        elif derivative == 2:
            q = 32 * (1 - 6 * u + 6 * u * u) / self.length_m**2
        else:
            raise ValueError("unsupported derivative")
        return self.base.evaluate(s, derivative) + self.amplitude * q


def bernstein_bound(coefficients, width):
    degree = len(coefficients) - 1
    controls = [
        sum(
            coefficients[j] * width**j * Fraction(math.comb(i, j), math.comb(degree, j))
            for j in range(i + 1)
        )
        for i in range(degree + 1)
    ]
    return max(map(abs, controls))


def upward_float(value):
    rounded = float(value)
    return math.nextafter(rounded, math.inf) if Fraction(rounded) < value else rounded


def limits(profile, amplitude):
    # Exact rational Bernstein hulls bound each polynomial interval; no dense-grid certificate.
    length = Fraction(float(profile.length_m))
    lift = Fraction(float(amplitude))
    max_grade, max_second = Fraction(0), Fraction(0)
    for lo, hi, cubic in zip(
        profile.breaks[:-1], profile.breaks[1:], profile.coefficients, strict=True
    ):
        lower, upper = Fraction(float(lo)), Fraction(float(hi))
        u = lower / length
        bump = [
            16 * u**2 * (1 - u) ** 2,
            32 * u * (1 - u) * (1 - 2 * u) / length,
            16 * (1 - 6 * u + 6 * u * u) / length**2,
            16 * (-2 + 4 * u) / length**3,
            16 / length**4,
        ]
        coefficients = [
            Fraction(float(value)) + lift * term
            for value, term in zip((*cubic, 0), bump, strict=True)
        ]
        first = [i * coefficients[i] for i in range(1, 5)]
        second = [i * first[i] for i in range(1, 4)]
        max_grade = max(max_grade, bernstein_bound(first, upper - lower))
        max_second = max(max_second, bernstein_bound(second, upper - lower))
    return upward_float(max_grade), upward_float(max_second)


def source_observations(
    anchor, axis, west, east, width, tangent, left, terrain, budget
):
    stations = (axis - axis[0]) @ tangent
    start, end = float(stations[west]), float(stations[east])
    estimated = math.ceil((end - start) / 100) + east - west + 2
    if 3 * estimated > budget.terrain_samples:
        raise ValueError("bridge_coarse_DEM_sample_budget")
    at = np.unique(np.r_[stations[west : east + 1], np.arange(start, end, 100.0)])
    centres = axis[0] + at[:, None] * tangent
    xy = (
        centres[:, None, :] + np.array([-width / 2, 0, width / 2])[None, :, None] * left
    ).reshape(-1, 2)
    geo = ground_geographic_at(anchor, xy)
    h = np.asarray(terrain(geo), float)
    if h.shape != (len(geo),) or not np.isfinite(h).all() or np.any(abs(h) > 1e6):
        raise ValueError("missing_or_invalid_bridge_coarse_DEM")
    return positions_in_frame(anchor, geo, h)


def clear_coarse_observations(
    profile, xyz, origin, tangent, left, s0, s1, planes, cross, grade, second
):
    s = (xyz[:, :2] - origin) @ tangent
    v = (xyz[:, :2] - origin) @ left
    inside = (s >= s0) & (s <= s1)
    at = s[inside] - s0
    base = np.empty(len(s))
    base[inside] = profile.evaluate(at)
    for index, mask in enumerate((s < s0, s > s1)):
        a, b = planes[index]
        base[mask] = a + b * s[mask]
    deficit = xyz[:, 2] - base - cross * v
    if np.any(deficit[~inside] > 1e-6):
        raise ValueError("landing_plane_below_coarse_DEM")
    u = at / profile.length_m
    bump = 16 * u**2 * (1 - u) ** 2
    use = bump > 1e-12
    if np.any(deficit[inside][~use] > 1e-6):
        raise ValueError("bridge_boundary_below_coarse_DEM")
    required = np.maximum(deficit[inside][use], 0) / bump[use]
    amplitude = float(required.max(initial=0))
    if amplitude:
        amplitude = float(np.nextafter(amplitude, np.inf))
    actual_grade, actual_second = limits(profile, amplitude)
    if actual_grade > grade or actual_second > second:
        raise ValueError("coarse_DEM_lift_exceeds_profile_derivative_bounds")
    lifted = LiftedProfile(profile, amplitude)
    base[inside] = lifted.evaluate(at)
    remaining = float(np.max(xyz[:, 2] - base - cross * v))
    if remaining > 1e-6:
        raise ValueError("coarse_DEM_lift_rounding_unresolved")
    return lifted, {
        "amplitude_m": amplitude,
        "maximum_observed_intrusion_before_m": float(deficit.max()),
        "maximum_observed_intrusion_after_m": max(0.0, remaining),
        "maximum_grade_bound": float(actual_grade),
        "maximum_second_derivative_bound_per_m": float(actual_second),
        "certificate": "Exact rational per-interval Bernstein convex hull, rounded outward",
        "coarse_DEM_samples": len(xyz),
        "source_step_m": 100.0,
        "scope": "Fixed source observations only; minimal lift in this fixed quartic family, no global optimum, full terrain or crossing/deck-clearance certificate",
    }
