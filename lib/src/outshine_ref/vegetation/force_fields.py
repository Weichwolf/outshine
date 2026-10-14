"""Own local geometric growth fields and radial attenuation, in metres and inverse metres."""

from dataclasses import dataclass

import numpy as np

from .profiles import Profile
from .spine_frames import unit


def point3(value):
    if not isinstance(value, tuple) or len(value) != 3:
        raise ValueError("immutable local three-component coordinates required")
    array = np.asarray(value)
    if (
        array.dtype.kind not in "fiu"
        or not np.isfinite(array).all()
        or np.max(abs(array)) > 1e6
    ):
        raise ValueError("finite bounded real local coordinates required")
    return tuple(float(v) for v in value)


def points3(value):
    value = np.asarray(value)
    if (
        value.dtype.kind not in "fiu"
        or value.ndim != 2
        or value.shape[1:] != (3,)
        or len(value) > 1_000_000
        or not np.isfinite(value).all()
        or np.max(abs(value), initial=0) > 1e6
    ):
        raise ValueError("bounded finite real local point rows required")
    return np.asarray(value, dtype=np.float64)


def field_inputs(rate, falloff):
    if (
        not isinstance(rate, Profile)
        or falloff is not None
        and not isinstance(falloff, RadialFalloff)
    ):
        raise TypeError("prepared rate profile and optional radial falloff required")


def profile_recipe(profile):
    return {"kind": profile.kind, "table": profile.table.tolist()}


@dataclass(frozen=True)
class RadialFalloff:
    centre: tuple[float, float, float]
    inner: float
    outer: float
    mode: str = "linear"

    def __post_init__(self):
        object.__setattr__(self, "centre", point3(self.centre))
        if (
            not np.isfinite((self.inner, self.outer)).all()
            or not 0 <= self.inner < self.outer <= 1e6
            or self.mode not in ("linear", "squared", "step")
        ):
            raise ValueError(
                "ordered finite radial range and declared falloff required"
            )

    def weights(self, points):
        distance = np.linalg.norm(points3(points) - self.centre, axis=1)
        if self.mode == "step":
            return (distance <= self.outer).astype(float)
        weight = 1 - np.clip((distance - self.inner) / (self.outer - self.inner), 0, 1)
        return weight * weight if self.mode == "squared" else weight

    def recipe(self):
        return {
            "centre": self.centre,
            "inner": self.inner,
            "outer": self.outer,
            "mode": self.mode,
        }


@dataclass(frozen=True)
class DirectionField:
    direction: tuple[float, float, float]
    rate: Profile
    falloff: RadialFalloff | None = None

    def __post_init__(self):
        raw = point3(self.direction)
        direction = unit(raw)
        object.__setattr__(self, "direction", raw)
        object.__setattr__(self, "unit_direction", tuple(float(v) for v in direction))
        field_inputs(self.rate, self.falloff)

    def evaluate(self, points, fraction):
        points = points3(points)
        weights = self.rate.evaluate(fraction)
        if self.falloff is not None:
            weights = weights * self.falloff.weights(points)
        return np.broadcast_to(weights, (len(points),))[:, None] * self.unit_direction

    def recipe(self):
        return {
            "kind": "direction",
            "direction": self.direction,
            "rate": profile_recipe(self.rate),
            "falloff": None if self.falloff is None else self.falloff.recipe(),
        }


@dataclass(frozen=True)
class AttractionField:
    target: tuple[float, float, float]
    rate: Profile
    softening: float = 0.05
    falloff: RadialFalloff | None = None

    def __post_init__(self):
        object.__setattr__(self, "target", point3(self.target))
        field_inputs(self.rate, self.falloff)
        if not np.isfinite(self.softening) or not 1e-6 <= self.softening <= 1e6:
            raise ValueError("positive finite attraction regularization required")

    def evaluate(self, points, fraction):
        delta = self.target - points3(points)
        direction = (
            delta
            / np.sqrt(np.sum(delta * delta, axis=1) + self.softening * self.softening)[
                :, None
            ]
        )
        weights = np.broadcast_to(self.rate.evaluate(fraction), (len(delta),))
        if self.falloff is not None:
            weights = weights * self.falloff.weights(points)
        return direction * weights[:, None]

    def recipe(self):
        return {
            "kind": "attraction",
            "target": self.target,
            "rate": profile_recipe(self.rate),
            "softening": self.softening,
            "falloff": None if self.falloff is None else self.falloff.recipe(),
        }


def field_from_recipe(recipe):
    record = dict(recipe)
    kind = record.pop("kind")
    profile = record.pop("rate")
    record["rate"] = Profile(profile["table"], profile["kind"])
    falloff = record.pop("falloff")
    record["falloff"] = (
        None
        if falloff is None
        else RadialFalloff(**(falloff | {"centre": tuple(falloff["centre"])}))
    )
    if kind == "direction":
        record["direction"] = tuple(record["direction"])
        return DirectionField(**record)
    if kind == "attraction":
        record["target"] = tuple(record["target"])
        return AttractionField(**record)
    raise ValueError("supported complete growth field recipe required")
