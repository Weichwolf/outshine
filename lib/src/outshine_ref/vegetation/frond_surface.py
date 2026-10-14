"""Stable guide deformation of own frond meshes with differential normals."""

import hashlib
import json
from dataclasses import dataclass, field
from functools import cached_property

import numpy as np

from . import frond_deform
from .curve_frame import arc_frame
from .profiles import Profile

MAX_VERTICES = frond_deform.MAX_VERTICES
MAX_TRIANGLES = frond_deform.MAX_TRIANGLES


def constant_profile():
    return Profile(np.ones(4), "maximum")


@dataclass(frozen=True)
class FrondGuide:
    length: float = 1.3
    bend: float = -0.65
    fold_per_m: float = 0.5
    width: Profile = field(default_factory=constant_profile)
    height: Profile = field(default_factory=constant_profile)
    roll: Profile = field(default_factory=lambda: Profile(np.zeros(4), "minimum"))

    def __post_init__(self):
        if not all(np.isfinite(v) for v in (self.length, self.bend, self.fold_per_m)):
            raise ValueError("finite physical frond guide required")
        if self.length <= 0 or abs(self.bend) > np.pi:
            raise ValueError("positive guide length and bounded bend required")
        if not all(
            isinstance(p, Profile) for p in (self.width, self.height, self.roll)
        ):
            raise ValueError("prepared immutable guide profiles required")
        if min(self.width.table.min(), self.height.table.min()) <= 0:
            raise ValueError("positive transverse scales preserve guide regularity")

    def recipe(self):
        return {
            "version": "frond-guide-1",
            "length_m": self.length,
            "bend_rad": self.bend,
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
        if recipe["version"] != "frond-guide-1":
            raise ValueError("supported complete guide recipe required")
        profiles = {
            name: Profile(np.array(value["table"]), value["kind"])
            for name, value in recipe["profiles"].items()
        }
        return cls(
            recipe["length_m"], recipe["bend_rad"], recipe["fold_per_m"], **profiles
        )

    @cached_property
    def identity(self):
        return hashlib.sha256(
            json.dumps(self.recipe(), sort_keys=True).encode()
        ).hexdigest()

    def validate(self, vertices):
        if not np.isfinite(vertices).all() or np.any(
            (vertices[:, 1] < 0) | (vertices[:, 1] > self.length)
        ):
            raise ValueError(
                "finite source vertices inside the complete guide required"
            )
        if not len(vertices):
            return
        extent = np.max(abs(vertices), axis=0)
        transverse = extent[0] * self.width.table.max()
        depth = (
            extent[2] * self.height.table.max() + abs(self.fold_per_m) * transverse**2
        )
        if abs(self.bend) * np.hypot(transverse, depth) >= self.length:
            raise ValueError(
                "source bounds must preserve the guide Jacobian orientation"
            )

    def evaluate(self, points):
        t = points[:, 1] / self.length
        centre, tangent, lateral, normal = arc_frame(self.length, self.bend, 0, t)
        angle, rate = self.roll.evaluate(t), self.roll.derivative(t)
        cosine, sine = np.cos(angle)[:, None], np.sin(angle)[:, None]
        a, b = cosine * lateral + sine * normal, -sine * lateral + cosine * normal
        da = rate[:, None] * b - self.bend * sine * tangent
        db = -rate[:, None] * a - self.bend * cosine * tangent
        width, height = self.width.evaluate(t), self.height.evaluate(t)
        q = points[:, 0] * width
        d = points[:, 2] * height + self.fold_per_m * q**2
        dq = points[:, 0] * self.width.derivative(t)
        dd = points[:, 2] * self.height.derivative(t) + 2 * self.fold_per_m * q * dq
        position = centre + q[:, None] * a + d[:, None] * b
        dx = width[:, None] * (a + (2 * self.fold_per_m * q)[:, None] * b)
        dy = (
            tangent
            + (dq[:, None] * a + q[:, None] * da + dd[:, None] * b + d[:, None] * db)
            / self.length
        )
        dz = height[:, None] * b
        return position, np.stack((dx, dy, dz), axis=-1)

    def deform(self, mesh, method="batch"):
        return frond_deform.deform_frond(self, mesh, method)
