"""Own frond cross-sections on prepared spatial spines and transported frames."""

import hashlib
import json
from dataclasses import dataclass, field
from functools import cached_property

import numpy as np

from .frond_deform import MAX_VERTICES, deform_frond
from .frond_surface import constant_profile
from .profiles import Profile
from .spine_curve import CubicSpine
from .spine_field import SpineField
from .spine_measure import ArcMeasure


@dataclass(frozen=True)
class SpatialFrondGuide:
    spine: CubicSpine
    width: Profile = field(default_factory=constant_profile)
    height: Profile = field(default_factory=constant_profile)
    roll: Profile = field(default_factory=lambda: Profile(np.zeros(4), "minimum"))
    fold_per_m: float = 0.0
    intervals_per_segment: int = 16

    def __post_init__(self):
        if not isinstance(self.spine, CubicSpine) or not all(
            isinstance(p, Profile) for p in (self.width, self.height, self.roll)
        ):
            raise TypeError("prepared spatial spine and immutable profiles required")
        if (
            not np.isfinite(self.fold_per_m)
            or min(self.width.table.min(), self.height.table.min()) <= 0
        ):
            raise ValueError("finite fold and positive transverse profiles required")
        centre, tangent, _, _ = self.spine.sample(0)
        if not np.allclose(centre, 0, rtol=0, atol=1e-12) or not np.allclose(
            tangent, (0, 1, 0), rtol=0, atol=1e-12
        ):
            raise ValueError("local guide starts at origin with positive Y tangent")
        self.field.sample(self.measure.fraction(self.measure.parameter_nodes))

    @cached_property
    def measure(self):
        return ArcMeasure(self.spine, self.intervals_per_segment)

    @cached_property
    def field(self):
        return SpineField(self.spine, self.measure, self.roll)

    @property
    def length(self):
        return self.measure.length

    @property
    def bend(self):
        return 0.0

    def recipe(self):
        return {
            "version": "spatial-frond-guide-1",
            "spine": self.spine.controls,
            "intervals_per_segment": self.intervals_per_segment,
            "fold_per_m": self.fold_per_m,
            "profiles": {
                name: {"kind": p.kind, "table": p.table.tolist()}
                for name, p in (
                    ("width", self.width),
                    ("height", self.height),
                    ("roll", self.roll),
                )
            },
        }

    @classmethod
    def from_recipe(cls, recipe):
        if (
            set(recipe)
            != {"version", "spine", "intervals_per_segment", "fold_per_m", "profiles"}
            or recipe["version"] != "spatial-frond-guide-1"
            or set(recipe["profiles"]) != {"width", "height", "roll"}
        ):
            raise ValueError("complete supported spatial frond recipe required")
        profiles = {
            name: Profile(value["table"], value["kind"])
            for name, value in recipe["profiles"].items()
        }
        return cls(
            CubicSpine(recipe["spine"]),
            **profiles,
            fold_per_m=recipe["fold_per_m"],
            intervals_per_segment=recipe["intervals_per_segment"],
        )

    @cached_property
    def identity(self):
        return hashlib.sha256(
            json.dumps(self.recipe(), sort_keys=True).encode()
        ).hexdigest()

    def validate(self, vertices):
        if (
            np.iscomplexobj(vertices)
            or vertices.ndim != 2
            or vertices.shape[1:] != (3,)
            or len(vertices) > MAX_VERTICES
            or not np.isfinite(vertices).all()
            or np.any((vertices[:, 1] < 0) | (vertices[:, 1] > self.length))
        ):
            raise ValueError(
                "bounded real source points inside the whole guide required"
            )
        if not len(vertices):
            return
        extent = np.max(abs(vertices), axis=0)
        q = extent[0] * self.width.table.max()
        d = extent[2] * self.height.table.max() + abs(self.fold_per_m) * q**2
        if self.spine.curvature_bound * np.hypot(q, d) >= 1:
            raise ValueError("source bounds must preserve spatial guide orientation")

    def evaluate(self, points):
        self.validate(points)
        fraction = points[:, 1] / self.length
        sample = self.field.sample(fraction)
        tangent, a, b = (sample[key] for key in ("tangent", "lateral", "normal"))
        curvature, speed, arc_speed, tau = (
            sample[key] for key in ("curvature", "speed", "arc_speed", "tau")
        )
        da = (
            -speed[:, None] * np.sum(curvature * a, axis=1)[:, None] * tangent
            + tau[:, None] * b
        ) / arc_speed[:, None]
        db = (
            -speed[:, None] * np.sum(curvature * b, axis=1)[:, None] * tangent
            - tau[:, None] * a
        ) / arc_speed[:, None]
        width, height = self.width.evaluate(fraction), self.height.evaluate(fraction)
        q = points[:, 0] * width
        d = points[:, 2] * height + self.fold_per_m * q**2
        dq = points[:, 0] * self.width.derivative(fraction) / self.length
        dd = (
            points[:, 2] * self.height.derivative(fraction) / self.length
            + 2 * self.fold_per_m * q * dq
        )
        position = sample["centre"] + q[:, None] * a + d[:, None] * b
        dx = width[:, None] * (a + (2 * self.fold_per_m * q)[:, None] * b)
        dy = (
            (speed / arc_speed)[:, None] * tangent
            + dq[:, None] * a
            + q[:, None] * da
            + dd[:, None] * b
            + d[:, None] * db
        )
        dz = height[:, None] * b
        return position, np.stack((dx, dy, dz), axis=-1)

    def deform(self, mesh, method="batch"):
        return deform_frond(self, mesh, method)
