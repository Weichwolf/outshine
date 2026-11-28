"""Bounded absolute-metre needle attachments on prepared circular twig axes."""

from dataclasses import asdict, dataclass
from types import MappingProxyType

import numpy as np

from .curve_frame import arc_frame
from .plan_arrays import detail_budget, immutable_array
from .plant_levels import GOLDEN_ANGLE
from .plant_plan import AxisState
from .shoot_plan import normal_frame


def rotate_y(angle):
    c, s = np.cos(angle), np.sin(angle)
    result = np.zeros(angle.shape + (3, 3))
    result[..., 0, 0] = c
    result[..., 2, 2] = c
    result[..., 0, 2] = s
    result[..., 2, 0] = -s
    result[..., 1, 1] = 1
    return result


@dataclass(frozen=True)
class NeedlePattern:
    spacing: float = 0.008
    group_size: int = 2
    first: float = 0.12
    last: float = 0.95
    pitch: float = 0.9
    spread: float = 0.0
    short_length: float = 0.002
    angle_variation: float = 0.12
    scale_variation: float = 0.1

    def __post_init__(self):
        if type(self.group_size) is not int or not 1 <= self.group_size <= 40:
            raise ValueError("one to 40 needles per group required")
        values = tuple(v for k, v in asdict(self).items() if k != "group_size")
        if (
            not np.isfinite(values).all()
            or not 1e-6 <= self.spacing <= 10
            or not 0 <= self.first <= self.last <= 1
            or not 0 <= self.short_length <= 0.02
            or not 0 < self.pitch < np.pi
            or not 0 <= self.spread < np.pi / 2
            or not 0 <= self.angle_variation < min(self.pitch, np.pi - self.pitch)
            or not 0 <= self.scale_variation < 0.5
        ):
            raise ValueError("bounded physical needle pattern required")

    def count(self, length):
        if not np.isfinite(length) or not 1e-6 <= length <= 1000:
            raise ValueError("bounded physical twig length required")
        groups = int(np.floor((self.last - self.first) * length / self.spacing)) + 1
        if groups > 4096:
            raise ValueError("needle group capacity exceeds 4096")
        return groups * self.group_size

    def detail(self, axis, radii, seed, max_needles):
        detail_budget(max_needles)
        if not isinstance(axis, AxisState):
            raise TypeError("prepared circular axis state required")
        if type(seed) is not int or not 0 <= seed < 2**64:
            raise ValueError("UInt64 needle pattern seed required")
        radii = np.asarray(radii)
        if (
            radii.shape != (2,)
            or radii.dtype.kind not in "fiu"
            or not np.isfinite(radii).all()
            or not 0 < radii[1] <= radii[0]
            or radii[0] > axis.length / 8
            or np.max(abs(axis.origin)) > 1e9
            or radii[0] * abs(axis.bend) >= axis.length
        ):
            raise ValueError("positive regular physical twig radii required")
        count = self.count(axis.length)
        if count > max_needles:
            return None
        groups = count // self.group_size
        rng = np.random.default_rng(np.random.SeedSequence((seed, axis.id)))
        t = self.first + np.arange(groups) * (self.spacing / axis.length)
        t = np.minimum(t, self.last)
        centre, tangent, lateral, normal = arc_frame(axis.length, axis.bend, 0, t)
        phi = rng.uniform(-np.pi, np.pi) + np.arange(groups) * GOLDEN_ANGLE
        phi += rng.uniform(-self.angle_variation, self.angle_variation, groups)
        radial = np.cos(phi)[:, None] * lateral + np.sin(phi)[:, None] * normal
        radius = radii[0] + (radii[1] - radii[0]) * t
        nodes = centre + radius[:, None] * radial
        pitch = self.pitch + rng.uniform(
            -self.angle_variation, self.angle_variation, groups
        )
        direction = np.cos(pitch)[:, None] * tangent + np.sin(pitch)[:, None] * radial
        short_frames = normal_frame(direction, tangent)
        world_nodes = nodes @ axis.frame.T + axis.origin
        world_short_frames = np.einsum("ij,njk->nik", axis.frame, short_frames)
        tips = world_nodes + self.short_length * world_short_frames[:, :, 1]
        member = np.tile(np.arange(self.group_size), groups)
        angles = member * (2 * np.pi / self.group_size)
        frames = np.einsum(
            "nij,njk->nik",
            np.repeat(short_frames, self.group_size, axis=0),
            rotate_y(angles),
        )
        if self.spread:
            d = (
                np.cos(self.spread) * frames[:, :, 1]
                + np.sin(self.spread) * frames[:, :, 2]
            )
            frames = normal_frame(d, frames[:, :, 0])
        result = {
            "ids": np.arange(count, dtype=np.uint64),
            "group_ids": np.repeat(np.arange(groups, dtype=np.uint32), self.group_size),
            "origins": np.repeat(tips, self.group_size, axis=0),
            "frames": np.einsum("ij,njk->nik", axis.frame, frames),
            "scales": 1
            + rng.uniform(-self.scale_variation, self.scale_variation, count),
            "short_origins": world_nodes,
            "short_frames": world_short_frames,
            "parent_parameters": t,
            "parent_radials": radial @ axis.frame.T,
        }
        return MappingProxyType({k: immutable_array(v) for k, v in result.items()})

    def recipe(self):
        return {"version": "needle-pattern-1", **asdict(self)}

    @classmethod
    def from_recipe(cls, recipe):
        if (
            not isinstance(recipe, dict)
            or set(recipe) != {"version", *cls.__dataclass_fields__}
            or recipe["version"] != "needle-pattern-1"
        ):
            raise ValueError("complete supported needle pattern recipe required")
        return cls(**{k: v for k, v in recipe.items() if k != "version"})
