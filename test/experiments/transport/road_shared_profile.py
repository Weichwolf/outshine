"""One scalar height/crossfall cubic over an unchanged segmented axis (#1542)."""

from dataclasses import dataclass

import numpy as np
from road_bank_profile import (
    RESIDUAL_LIMIT,
    grade_residual,
    outer_cubic,
    prior_controls,
)
from road_banked_surface import BankedSurface
from road_surface_model import Curve, bezier, controls


@dataclass(frozen=True)
class ProfileRecipe:
    basis: str
    height: np.ndarray
    bank: np.ndarray

    def surface(self, source):
        if self.basis == "shared-cubic":
            return SharedCubicSurface(source, self.height, self.bank)
        if self.basis == "source-cubics":
            return BankedSurface(source, self.bank, height_controls=self.height)
        raise ValueError("Explicit shared-cubic or source-cubics basis required")


def restriction(control, parameter, total_length, lengths):
    values = bezier(control, parameter)
    rates = bezier(3 * np.diff(control) / total_length, parameter)
    return controls(values[:-1], values[1:], rates[:-1], rates[1:], lengths).T


def rate_restriction(control, parameter, total_length):
    rate = 3 * np.diff(control) / total_length
    first = bezier(rate, parameter[:-1])
    last = bezier(rate, parameter[1:])
    middle = first + np.diff(parameter) * bezier(2 * np.diff(rate), parameter[:-1]) / 2
    return np.c_[first, middle, last]


def shared_candidate(curve, limits, height_change):
    length = curve.stations[-1] - curve.stations[0]
    parameter = (curve.stations - curve.stations[0]) / length
    prior_bank = prior_controls(curve)
    height = outer_cubic(curve, curve.height)
    bank = outer_cubic(curve, prior_bank)
    local_height = restriction(height, parameter, length, curve.lengths)
    local_bank = restriction(bank, parameter, length, curve.lengths)
    h_rate = rate_restriction(height, parameter, length)
    q_rate = rate_restriction(bank, parameter, length)
    if not all(np.isfinite(a).all() for a in (height, bank, h_rate, q_rate)):
        return None, {"reason": "shared_nonfinite"}
    height_change_bound = float(abs(local_height - curve.height).max())
    bank_change_bound = float(abs(local_bank - prior_bank).max())
    outer_error = 0.0
    for prior, control in ((curve.height, height), (prior_bank, bank)):
        old = 3 * np.diff(prior, axis=1) / curve.lengths[:, None]
        new = 3 * np.diff(control) / length
        outer_error = max(
            outer_error,
            abs(control[0] - prior[0, 0]),
            abs(control[-1] - prior[-1, -1]),
            abs(new[0] - old[0, 0]),
            abs(new[-1] - old[-1, -1]),
        )
    errors = {
        "grade": grade_residual(curve, limits, h_rate, q_rate, "quadratic"),
        "height_allowance": height_change_bound - height_change,
        "crossfall_allowance": bank_change_bound - limits.change,
        "crossfall": float(abs(local_bank).max()) - limits.crossfall,
        "outer_profiles": float(outer_error),
    }
    error = max(errors.values())
    note = {
        "maximum_numeric_residual": error,
        "residual_components": errors,
        "maximum_height_control_change_m": height_change_bound,
        "maximum_bank_control_change": bank_change_bound,
    }
    if error > RESIDUAL_LIMIT:
        return None, note | {"reason": "shared_unqualified"}
    return (height, bank), note | {"reason": ""}


class SharedCubicSurface(Curve):
    def __init__(self, source, height, bank):
        super().__init__(source)
        self.height = np.array(height, dtype=float, copy=True)
        self.crossfall_controls = np.array(bank, dtype=float, copy=True)
        if any(
            a.shape != (4,) or not np.isfinite(a).all()
            for a in (self.height, self.crossfall_controls)
        ):
            raise ValueError("Two explicit finite shared scalar cubics required")
        self.profile_length = self.stations[-1] - self.stations[0]
        parameter = (self.stations - self.stations[0]) / self.profile_length
        second = []
        for control in (self.height, self.crossfall_controls):
            derivative = 6 * np.diff(control, n=2) / self.profile_length**2
            endpoints = abs(bezier(derivative, parameter))
            second.append(np.maximum(endpoints[:-1], endpoints[1:]))
        planar = np.hypot(
            self.width * abs(self.curvature_rate),
            self.curvature_max * (1 + self.width * self.curvature_max),
        )
        self.second = np.hypot(planar, second[0] + self.width * second[1])
        if not np.isfinite(self.second).all():
            raise ValueError("Finite shared surface derivative bounds required")

    def _profile(self, control, indices, parameter, derivative):
        t = (
            self.stations[indices]
            - self.stations[0]
            + parameter * self.lengths[indices]
        ) / self.profile_length
        if derivative == 0:
            return bezier(control, t)
        if derivative == 1:
            return bezier(3 * np.diff(control) / self.profile_length, t)
        raise ValueError("Shared scalar value or first derivative required")

    def height_at(self, indices, parameter, *, derivative=0):
        return self._profile(self.height, indices, parameter, derivative)

    def crossfall(self, indices, parameter, *, derivative=0):
        return self._profile(self.crossfall_controls, indices, parameter, derivative)
