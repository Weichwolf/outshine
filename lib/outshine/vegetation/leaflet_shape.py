"""Immutable local leaflet profiles and stable section-level shape variation."""

from dataclasses import dataclass, field

import numpy as np

from .profiles import Profile


def zero_profile():
    return Profile(np.zeros(4), "minimum")


@dataclass(frozen=True)
class LeafletShape:
    bend: Profile = field(default_factory=zero_profile)
    fold: Profile = field(default_factory=zero_profile)
    twist: Profile = field(default_factory=zero_profile)
    variance: tuple[float, float, float] = (0.0, 0.0, 0.0)

    def __post_init__(self):
        if not all(isinstance(p, Profile) for p in (self.bend, self.fold, self.twist)):
            raise TypeError("prepared immutable leaflet profiles required")
        if (
            not isinstance(self.variance, tuple)
            or len(self.variance) != 3
            or any(
                not isinstance(v, (int, float, np.integer, np.floating))
                for v in self.variance
            )
            or not np.isfinite(self.variance).all()
            or any(v < 0 or isinstance(v, (bool, np.bool_)) for v in self.variance)
        ):
            raise ValueError("three bounded nonnegative shape variations required")
        if self.maximum[0] > np.pi or self.maximum[1] > 2 or self.maximum[2] > np.pi:
            raise ValueError("bounded bend/cup/twist profiles and variation required")

    @property
    def maximum(self):
        return tuple(
            float(np.max(abs(p.table))) + v
            for p, v in zip(
                (self.bend, self.fold, self.twist), self.variance, strict=True
            )
        )

    def sample(self, parameter, seed, identity):
        if (
            not np.isfinite(parameter)
            or not 0 <= parameter <= 1
            or type(seed) is not int
            or not 0 <= seed < 2**64
            or type(identity) is not int
            or identity < 0
            or identity > 128
        ):
            raise ValueError(
                "bounded organ parameter, stable seed and section ID required"
            )
        base = np.array(
            [p.evaluate(parameter) for p in (self.bend, self.fold, self.twist)]
        )
        if any(self.variance):
            rng = np.random.default_rng(
                np.random.SeedSequence((seed, identity, 0x4C454146))
            )
            base += rng.uniform(-1, 1, 3) * self.variance
        return tuple(float(v) for v in base)

    def recipe(self):
        return {
            "version": "leaflet-shape-1",
            "profiles": {
                name: {"kind": p.kind, "table": p.table.tolist()}
                for name, p in (
                    ("bend", self.bend),
                    ("fold", self.fold),
                    ("twist", self.twist),
                )
            },
            "variance": list(self.variance),
        }

    @classmethod
    def from_recipe(cls, recipe):
        if (
            set(recipe) != {"version", "profiles", "variance"}
            or recipe["version"] != "leaflet-shape-1"
            or set(recipe["profiles"]) != {"bend", "fold", "twist"}
        ):
            raise ValueError("complete supported leaflet shape recipe required")
        profiles = {
            name: Profile(value["table"], value["kind"])
            for name, value in recipe["profiles"].items()
        }
        return cls(**profiles, variance=tuple(recipe["variance"]))
