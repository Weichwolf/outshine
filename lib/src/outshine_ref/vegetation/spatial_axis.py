"""Complete authored spatial axis recipes and shared continuous sweep products."""

from dataclasses import dataclass, field
from functools import cached_property

import numpy as np

from .profiles import Profile
from .spine_curve import CubicSpine
from .spine_field import SpineField
from .spine_measure import ArcMeasure
from .spine_surface import sweep_stations
from .tube_topology import close_rings


@dataclass(frozen=True)
class SpatialAxis:
    spine: CubicSpine
    radius: Profile
    roll: Profile = field(default_factory=lambda: Profile(np.zeros(4)))
    ellipse: tuple = (1.0, 1.0)
    intervals_per_segment: int = 16

    def __post_init__(self):
        if (
            not isinstance(self.spine, CubicSpine)
            or not isinstance(self.radius, Profile)
            or not isinstance(self.roll, Profile)
        ):
            raise TypeError("prepared spatial curve, radius and roll profiles required")
        if (
            np.min(self.radius.table) <= 0
            or not isinstance(self.ellipse, tuple)
            or len(self.ellipse) != 2
            or not np.isfinite(self.ellipse).all()
            or min(self.ellipse) <= 0
        ):
            raise ValueError(
                "positive spatial radii and immutable elliptical axes required"
            )
        if (
            self.spine.curvature_bound * max(self.ellipse) * self.radius.table.max()
            >= 1
        ):
            raise ValueError("spatial tube exceeds its local regularity certificate")
        centre, tangent, _, _ = self.spine.sample(0)
        if not np.allclose(centre, 0, rtol=0, atol=1e-12) or not np.allclose(
            tangent, (0, 1, 0), rtol=0, atol=1e-12
        ):
            raise ValueError("local axis must start at origin with positive Y tangent")

    @cached_property
    def measure(self):
        return ArcMeasure(self.spine, self.intervals_per_segment)

    @property
    def length(self):
        return self.measure.length

    @property
    def bend(self):
        return 0.0

    @cached_property
    def field(self):
        return SpineField(self.spine, self.measure, self.roll)

    def recipe(self):
        return {
            "spine": self.spine.controls,
            "radius": {"kind": self.radius.kind, "table": self.radius.table.tolist()},
            "roll": {"kind": self.roll.kind, "table": self.roll.table.tolist()},
            "ellipse": self.ellipse,
            "intervals_per_segment": self.intervals_per_segment,
        }

    @classmethod
    def from_recipe(cls, record):
        return cls(
            CubicSpine(record["spine"]),
            Profile(np.asarray(record["radius"]["table"]), record["radius"]["kind"]),
            Profile(np.asarray(record["roll"]["table"]), record["roll"]["kind"]),
            tuple(record["ellipse"]),
            record["intervals_per_segment"],
        )

    def mesh(self, segments=48, sides=12):
        if (
            type(segments) is not int
            or not 2 <= segments <= 1024
            or type(sides) is not int
            or not 3 <= sides <= 128
        ):
            raise ValueError(
                "bounded spatial axial and angular surface detail required"
            )
        fraction = np.linspace(0, 1, segments + 1)
        with np.errstate(over="ignore"):
            variation = np.r_[0, np.cumsum(np.abs(np.diff(self.roll.table)))]
        if not np.isfinite(variation).all():
            raise ValueError(
                "finite representable cross-section roll variation required"
            )
        sampled = np.interp(fraction, np.linspace(0, 1, len(variation)), variation)
        if np.any(np.diff(sampled) > np.pi / 2):
            raise ValueError("tube subdivision does not resolve cross-section roll")
        sample = self.field.sample(fraction)
        frames = np.stack(
            (sample["lateral"], sample["tangent"], sample["normal"]), axis=1
        )
        radius = self.radius.evaluate(fraction)
        slope = self.radius.derivative(fraction) * sample["arc_speed"] / self.length
        angle = np.arange(sides) * 2 * np.pi / sides
        surface, normals = sweep_stations(
            sample["centre"],
            sample["tangent"],
            sample["speed"],
            sample["curvature"],
            frames,
            radius,
            slope,
            np.asarray(self.ellipse),
            sample["tau"],
            angle,
        )
        mesh = close_rings(surface, normals, sample["centre"], sample["tangent"])
        q = radius[:, None] * self.ellipse[0] * np.cos(angle)
        d = radius[:, None] * self.ellipse[1] * np.sin(angle)
        s = np.broadcast_to((fraction * self.length)[:, None], q.shape)
        coordinates = np.column_stack((q.ravel(), d.ravel(), s.ravel()))
        coordinates = np.vstack((coordinates, (0, 0, 0), (0, 0, self.length)))
        material_radius = np.r_[
            np.repeat(radius, sides), radius[0], radius[-1]
        ] * np.sqrt(np.prod(self.ellipse))
        return mesh | {"bark_coordinates": coordinates, "bark_radius": material_radius}
