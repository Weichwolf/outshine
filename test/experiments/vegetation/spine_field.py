"""Continuous transported frames with exact derivatives of the prepared field."""

from dataclasses import dataclass

import numpy as np
from scipy.interpolate import CubicHermiteSpline
from spine_frames import transport_frames


@dataclass(frozen=True)
class SpineField:
    spine: object
    measure: object
    roll: object

    def __post_init__(self):
        t = self.measure.parameter_nodes
        centre, tangent, speed, curvature = self.spine.sample(t)
        reference = transport_frames(centre, tangent)[:, 0]
        derivative = (
            -np.sum(reference * curvature, axis=-1)[:, None] * speed[:, None] * tangent
        )
        object.__setattr__(
            self, "lateral_curve", CubicHermiteSpline(t, reference, derivative)
        )

    def sample(self, fraction):
        fraction = np.asarray(fraction, dtype=float)
        t = self.measure.parameter(fraction)
        centre, tangent, speed, curvature = self.spine.sample(t)
        tangent_d = speed[..., None] * curvature
        v = self.lateral_curve(t)
        v_d = self.lateral_curve(t, nu=1)
        dot = np.sum(v * tangent, axis=-1)
        a = v - dot[..., None] * tangent
        norm = np.linalg.norm(a, axis=-1)
        if np.any(norm < 0.5):
            raise ValueError("transport field requires more resolved stations")
        lateral = a / norm[..., None]
        a_d = (
            v_d
            - (np.sum(v_d * tangent + v * tangent_d, axis=-1))[..., None] * tangent
            - dot[..., None] * tangent_d
        )
        lateral_d = (a_d - np.sum(a_d * lateral, axis=-1)[..., None] * lateral) / norm[
            ..., None
        ]
        normal = np.cross(lateral, tangent)
        arc_speed = self.measure.polynomial(t, nu=1)
        angle = self.roll.evaluate(fraction)
        roll_rate = self.roll.derivative(fraction) * arc_speed / self.measure.length
        tau = np.sum(lateral_d * normal, axis=-1) + roll_rate
        rolled = np.cos(angle)[..., None] * lateral + np.sin(angle)[..., None] * normal
        normal = np.cross(rolled, tangent)
        return {
            "parameter": t,
            "centre": centre,
            "tangent": tangent,
            "lateral": rolled,
            "normal": normal,
            "speed": speed,
            "arc_speed": arc_speed,
            "curvature": curvature,
            "tau": tau,
        }
