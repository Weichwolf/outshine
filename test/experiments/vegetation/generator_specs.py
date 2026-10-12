"""Immutable geometry and placement recipes for composed vegetation generators."""

from dataclasses import asdict, dataclass, field

import numpy as np
from blade_surface import Blade
from frond_pattern import PinnatePattern
from frond_surface import FrondGuide
from profile_curve import Profile


def profile_recipe(profile):
    return {"kind": profile.kind, "table": profile.table.tolist()}


def profile_from_recipe(recipe):
    return Profile(np.asarray(recipe["table"]), recipe["kind"])


@dataclass(frozen=True)
class TubeShape:
    length: float = 0.6
    bend: float = 0.2
    radius: Profile = field(
        default_factory=lambda: Profile(np.linspace(0.006, 0.0015, 4))
    )

    def __post_init__(self):
        if (
            not all(np.isfinite(v) for v in (self.length, self.bend))
            or self.length <= 0
            or abs(self.bend) > np.pi
        ):
            raise ValueError("finite positive axis length and bounded bend required")
        if not isinstance(self.radius, Profile) or self.radius.table.min() <= 0:
            raise ValueError("positive physical radius profile required")
        if abs(self.bend) * self.radius.table.max() >= self.length:
            raise ValueError("regular complete axis shape required")

    def recipe(self):
        return {
            "length": self.length,
            "bend": self.bend,
            "radius": profile_recipe(self.radius),
        }


@dataclass(frozen=True)
class FrondShape:
    pattern: PinnatePattern = field(default_factory=PinnatePattern)
    guide: FrondGuide = field(default_factory=FrondGuide)

    def __post_init__(self):
        if (
            not isinstance(self.pattern, PinnatePattern)
            or not isinstance(self.guide, FrondGuide)
            or self.pattern.length != self.guide.length
        ):
            raise ValueError("one complete pattern and matching frond guide required")
        transverse = max(
            self.pattern.leaflet_length * (1 + self.pattern.variance),
            self.pattern.length * (1 - self.pattern.last),
        ) * (1 + self.pattern.aspect / 2)
        self.guide.validate(
            np.array(((transverse, 0, 0.004), (-transverse, self.guide.length, -0.004)))
        )

    @property
    def length(self):
        return self.guide.length

    @property
    def bend(self):
        return self.guide.bend

    def recipe(self):
        return {"pattern": self.pattern.recipe(), "guide": self.guide.recipe()}


@dataclass(frozen=True)
class Placement:
    count: int = 1
    first: float = 0.15
    last: float = 0.95
    pitch: float = 1.1
    pitch_variance: float = 0.08
    azimuth_step: float = np.pi * (3 - np.sqrt(5))
    azimuth_variance: float = 0.08
    scale: Profile = field(default_factory=lambda: Profile(np.ones(4), "maximum"))
    scale_variance: float = 0.04

    def __post_init__(self):
        values = (
            self.first,
            self.last,
            self.pitch,
            self.pitch_variance,
            self.azimuth_step,
            self.azimuth_variance,
            self.scale_variance,
        )
        if (
            type(self.count) is not int
            or not 0 <= self.count <= 64
            or not all(np.isfinite(v) for v in values)
        ):
            raise ValueError(
                "finite placement with zero to 64 children per parent required"
            )
        if not 0 <= self.first <= self.last <= 1 or not 0 <= self.pitch_variance <= min(
            self.pitch, np.pi - self.pitch
        ):
            raise ValueError("ordered parent interval and valid pitch range required")
        if (
            not 0 <= self.azimuth_variance <= np.pi
            or not 0 <= self.scale_variance < 0.5
        ):
            raise ValueError("bounded angular and scale variation required")
        if not isinstance(self.scale, Profile) or self.scale.table.min() <= 0:
            raise ValueError("positive prepared placement scale profile required")

    def parameter(self, slot):
        fraction = slot / (self.count - 1) if self.count > 1 else 0.5
        return self.first + fraction * (self.last - self.first)

    def recipe(self):
        result = {key: value for key, value in self.__dict__.items() if key != "scale"}
        return result | {"scale": profile_recipe(self.scale)}


@dataclass(frozen=True)
class Generator:
    id: int
    label: str
    kind: str
    parents: tuple[int, ...]
    shape: TubeShape | Blade | FrondShape | None = None
    placement: Placement = field(default_factory=Placement)

    def __post_init__(self):
        if (
            type(self.id) is not int
            or not 0 <= self.id < 2**32
            or not isinstance(self.label, str)
            or not 1 <= len(self.label) <= 80
        ):
            raise ValueError("stable generator ID and bounded label required")
        if (
            not isinstance(self.parents, tuple)
            or len(set(self.parents)) != len(self.parents)
            or any(type(p) is not int or not 0 <= p < 2**32 for p in self.parents)
        ):
            raise ValueError("distinct immutable parent generator IDs required")
        expected = {
            "root": type(None),
            "axis": TubeShape,
            "blade": Blade,
            "frond": FrondShape,
        }
        if (
            self.kind not in expected
            or not isinstance(self.shape, expected[self.kind])
            or not isinstance(self.placement, Placement)
        ):
            raise ValueError(
                "implemented generator kind and matching geometry recipe required"
            )
        if (
            self.kind == "root"
            and (self.parents or self.placement.count != 1)
            or self.kind != "root"
            and not self.parents
        ):
            raise ValueError(
                "one parentless root and connected geometry generators required"
            )

    def recipe(self):
        shape = (
            asdict(self.shape)
            if self.kind == "blade"
            else self.shape.recipe()
            if self.shape is not None
            else None
        )
        return {
            "id": self.id,
            "label": self.label,
            "kind": self.kind,
            "parents": sorted(self.parents),
            "shape": shape,
            "placement": self.placement.recipe(),
        }

    @classmethod
    def from_recipe(cls, recipe):
        kind, record = recipe["kind"], recipe["shape"]
        if kind == "root":
            shape = None
        elif kind == "axis":
            shape = TubeShape(
                record["length"], record["bend"], profile_from_recipe(record["radius"])
            )
        elif kind == "blade":
            shape = Blade(**record)
        elif kind == "frond":
            pattern = dict(record["pattern"])
            if pattern.pop("version") != "pinnate-source-1":
                raise ValueError("supported frond pattern required")
            shape = FrondShape(
                PinnatePattern(**pattern), FrondGuide.from_recipe(record["guide"])
            )
        else:
            raise ValueError("implemented generator kind required")
        placement = dict(recipe["placement"])
        placement["scale"] = profile_from_recipe(placement["scale"])
        return cls(
            recipe["id"],
            recipe["label"],
            kind,
            tuple(recipe["parents"]),
            shape,
            Placement(**placement),
        )
