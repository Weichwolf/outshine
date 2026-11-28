"""Own fixed/moving-axis turning, planar heading and initial-direction recovery."""

from dataclasses import dataclass

import numpy as np

from .force_fields import RadialFalloff, field_inputs, point3, points3, profile_recipe
from .profiles import Profile
from .spine_frames import unit


def vectors3(value, count):
    if value is None:
        raise ValueError("explicit heading/frame vectors required")
    value = np.asarray(value)
    if (
        value.dtype.kind not in "fiu"
        or value.shape != (count, 3)
        or not np.isfinite(value).all()
    ):
        raise ValueError("one finite real vector per point required")
    return np.asarray(value, dtype=np.float64)


def weighted(field, points, fraction, vectors):
    weights = np.broadcast_to(field.rate.evaluate(fraction), (len(points),))
    if field.falloff is not None:
        weights = weights * field.falloff.weights(points)
    return weights[:, None] * vectors


@dataclass(frozen=True)
class CurlField:
    axis: tuple[float, float, float]
    rate: Profile
    falloff: RadialFalloff | None = None

    def __post_init__(self):
        raw = point3(self.axis)
        object.__setattr__(self, "axis", raw)
        object.__setattr__(self, "unit_axis", tuple(float(v) for v in unit(raw)))
        field_inputs(self.rate, self.falloff)

    def evaluate(self, points, fraction, headings, lateral):
        points = points3(points)
        h = vectors3(headings, len(points))
        a = vectors3(lateral, len(points))
        b = np.cross(a, h)
        x, y, z = self.unit_axis
        axis = x * a + y * h + z * b
        return weighted(self, points, fraction, np.cross(axis, h))

    def recipe(self):
        return {
            "kind": "curl",
            "axis": self.axis,
            "rate": profile_recipe(self.rate),
            "falloff": None if self.falloff is None else self.falloff.recipe(),
        }


@dataclass(frozen=True)
class GnarlField:
    axis: tuple[float, float, float]
    rate: Profile
    falloff: RadialFalloff | None = None

    def __post_init__(self):
        raw = point3(self.axis)
        object.__setattr__(self, "axis", raw)
        object.__setattr__(self, "unit_axis", tuple(float(v) for v in unit(raw)))
        field_inputs(self.rate, self.falloff)

    def evaluate(self, points, fraction, headings):
        points = points3(points)
        return weighted(
            self,
            points,
            fraction,
            np.cross(self.unit_axis, vectors3(headings, len(points))),
        )

    def recipe(self):
        return {
            "kind": "gnarl",
            "axis": self.axis,
            "rate": profile_recipe(self.rate),
            "falloff": None if self.falloff is None else self.falloff.recipe(),
        }


@dataclass(frozen=True)
class PlanarField:
    normal: tuple[float, float, float]
    rate: Profile
    falloff: RadialFalloff | None = None

    def __post_init__(self):
        raw = point3(self.normal)
        object.__setattr__(self, "normal", raw)
        object.__setattr__(self, "unit_normal", tuple(float(v) for v in unit(raw)))
        field_inputs(self.rate, self.falloff)

    def evaluate(self, points, fraction, headings):
        points = points3(points)
        h = vectors3(headings, len(points))
        n = np.asarray(self.unit_normal)
        projection = h - np.sum(h * n, axis=1)[:, None] * n
        return weighted(self, points, fraction, projection)

    def recipe(self):
        return {
            "kind": "planar",
            "normal": self.normal,
            "rate": profile_recipe(self.rate),
            "falloff": None if self.falloff is None else self.falloff.recipe(),
        }


@dataclass(frozen=True)
class ReturnField:
    rate: Profile
    falloff: RadialFalloff | None = None

    def __post_init__(self):
        field_inputs(self.rate, self.falloff)

    def evaluate(self, points, fraction, reference):
        points = points3(points)
        initial = unit(reference)
        if initial.shape != (3,):
            raise ValueError("one declared initial direction required")
        return weighted(self, points, fraction, np.broadcast_to(initial, points.shape))

    def recipe(self):
        return {
            "kind": "return",
            "rate": profile_recipe(self.rate),
            "falloff": None if self.falloff is None else self.falloff.recipe(),
        }


def turning_from_recipe(kind, record):
    types = {
        "curl": CurlField,
        "gnarl": GnarlField,
        "planar": PlanarField,
        "return": ReturnField,
    }
    if kind not in types:
        raise ValueError("supported complete growth field recipe required")
    for key in ("axis", "normal"):
        if key in record:
            record[key] = tuple(record[key])
    return types[kind](**record)
