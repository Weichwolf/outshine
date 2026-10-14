"""Immutable branching-order recipes for circular-axis plant plans."""

from dataclasses import asdict, dataclass

import numpy as np

from .blade_surface import Blade
from .shoot_plan import Shoot

GOLDEN_ANGLE = np.pi * (3 - np.sqrt(5))


@dataclass(frozen=True)
class AxisLevel:
    branches: int
    length: float
    base_radius: float
    tip_radius: float
    tip_scale: float = 0.55
    length_shape: str = "taper"
    attachment_jitter: float = 0.0
    azimuth_variation: float = 0.18
    upward_bias: float = 0.0
    first: float = 0.2
    last: float = 0.92
    angle: float = 1.1
    angle_variation: float = 0.18
    bend: float = 0.35
    bend_variation: float = 0.1
    leaf_internode: float | None = None
    leaf_first: float = 0.1
    leaf_last: float = 0.96
    leaf_angle: float = 1.12
    leaf_sky_alignment: float = 0.65
    arrangement: str = "spiral"
    whorl_size: int = 1
    angle_tip: float | None = None

    def __post_init__(self):
        if type(self.branches) is not int or not 1 <= self.branches <= 64:
            raise ValueError("one to 64 branches per order required")
        if type(self.whorl_size) is not int or not 1 <= self.whorl_size <= 16:
            raise ValueError("one to 16 branches per whorl required")
        values = [v for v in asdict(self).values() if isinstance(v, (int, float))]
        if not all(np.isfinite(v) for v in values):
            raise ValueError("finite axis parameters required")
        if (
            not 0 < self.tip_radius <= self.base_radius
            or not 0.05 <= self.tip_scale <= 1
        ):
            raise ValueError("positive taper and bounded length profile required")
        if self.length_shape not in ("taper", "rounded"):
            raise ValueError("declared branch length profile required")
        if not 0 <= self.attachment_jitter < 0.5 or not 0 <= self.upward_bias < 0.5:
            raise ValueError("bounded attachment jitter and upward bias required")
        if not 0 <= self.azimuth_variation <= np.pi:
            raise ValueError("bounded azimuth variation required")
        curvature = abs(self.bend) + self.bend_variation
        if self.bend_variation < 0 or curvature > np.pi:
            raise ValueError("bounded bend variation required")
        if self.length * self.tip_scale <= curvature * self.base_radius:
            raise ValueError("length profile must preserve regular tubes")
        if not 0 <= self.first <= self.last <= 1 or self.angle_variation < 0:
            raise ValueError("valid parent interval and angular variation required")
        if not (
            0 < self.angle - self.angle_variation
            and self.angle + self.angle_variation < np.pi
            and (
                self.angle_tip is None
                or 0 < self.angle_tip - self.angle_variation
                and self.angle_tip + self.angle_variation < np.pi
            )
        ):
            raise ValueError("branch inclinations must stay between zero and pi")
        if not 0 <= self.leaf_sky_alignment < 1:
            raise ValueError("bounded leaf alignment required")
        self.shoot(0, 0)

    def length_at(self, slot):
        group, groups = (
            slot // self.whorl_size,
            (self.branches - 1) // self.whorl_size + 1,
        )
        fraction = group / (groups - 1) if groups > 1 else 0
        if self.length_shape == "rounded":
            return self.length * (
                self.tip_scale + (1 - self.tip_scale) * np.sin(np.pi * fraction)
            )
        return self.length * (1 + (self.tip_scale - 1) * fraction)

    def attachment(self, slot, jitter=0.0):
        group, groups = (
            slot // self.whorl_size,
            (self.branches - 1) // self.whorl_size + 1,
        )
        fraction = group / (groups - 1) if groups > 1 else 0.5
        fraction += jitter * self.attachment_jitter / max(1, groups - 1)
        return self.first + (self.last - self.first) * np.clip(fraction, 0, 1)

    def azimuth_at(self, slot):
        if self.whorl_size == 1:
            return slot * GOLDEN_ANGLE
        group, member = divmod(slot, self.whorl_size)
        return group * GOLDEN_ANGLE + member * (2 * np.pi / self.whorl_size)

    def inclination_at(self, slot):
        if self.angle_tip is None:
            return self.angle
        group, groups = (
            slot // self.whorl_size,
            (self.branches - 1) // self.whorl_size + 1,
        )
        fraction = group / (groups - 1) if groups > 1 else 0.5
        return self.angle + (self.angle_tip - self.angle) * fraction

    def shoot(self, slot, seed, bend=None, blade=None, normal_hint=None):
        return Shoot(
            seed=seed,
            length=self.length_at(slot),
            internode=self.leaf_internode if self.leaf_internode is not None else 1.0,
            bend=self.bend if bend is None else bend,
            first=self.leaf_first,
            last=self.leaf_last,
            leaf_angle=self.leaf_angle,
            arrangement=self.arrangement,
            blade=Blade() if blade is None else blade,
            normal_hint=normal_hint,
            normal_alignment=0 if normal_hint is None else self.leaf_sky_alignment,
        )

    def leaf_count(self, slot):
        return 0 if self.leaf_internode is None else self.shoot(slot, 0).count
