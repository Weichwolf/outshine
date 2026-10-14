"""A finite metamer group with stable phyllotaxis and deferred organ placements."""

import hashlib
import json
from dataclasses import asdict, dataclass, field
from functools import cached_property
from types import MappingProxyType

import numpy as np

from .blade_surface import Blade
from .curve_frame import arc_frame
from .plan_arrays import detail_budget, immutable_array


def normal_frame(direction, hint):
    direction, hint = np.asarray(direction, dtype=float), np.asarray(hint, dtype=float)
    if (
        direction.ndim not in (1, 2)
        or direction.shape[-1] != 3
        or hint.ndim not in (1, 2)
        or hint.shape[-1] != 3
        or not np.isfinite(direction).all()
        or not np.isfinite(hint).all()
    ):
        raise ValueError("finite three-dimensional directions and hints required")
    length = np.linalg.norm(direction, axis=-1, keepdims=True)
    if not np.isfinite(length).all() or np.any(length == 0):
        raise ValueError("finite representable nonzero directions required")
    direction = direction / length
    hint = np.broadcast_to(hint, direction.shape)
    lateral = np.cross(direction, hint)
    fallback = np.eye(3)[np.argmin(np.abs(direction), axis=-1)]
    fallback = np.cross(direction, fallback)
    lateral = np.where(
        np.linalg.norm(lateral, axis=-1, keepdims=True) < 1e-8, fallback, lateral
    )
    lateral /= np.linalg.norm(lateral, axis=-1, keepdims=True)
    normal = np.cross(lateral, direction)
    return np.stack((lateral, direction, normal), axis=-1)


@dataclass(frozen=True)
class Shoot:
    seed: int = 17
    length: float = 0.48
    internode: float = 0.04
    bend: float = 0.3
    first: float = 0.06
    last: float = 0.96
    arrangement: str = "spiral"
    whorl: int = 3
    leaf_angle: float = 1.12
    normal_hint: tuple[float, float, float] | None = None
    normal_alignment: float = 0.0
    blade: Blade = field(default_factory=lambda: Blade(lobes=6))

    def __post_init__(self):
        if type(self.seed) is not int or not 0 <= self.seed < 2**64:
            raise ValueError("unsigned 64-bit seed required")
        if not all(
            np.isfinite(v)
            for v in (
                self.length,
                self.internode,
                self.bend,
                self.first,
                self.last,
                self.leaf_angle,
                self.normal_alignment,
            )
        ):
            raise ValueError("finite shoot parameters required")
        if self.length <= 0 or self.internode <= 0 or abs(self.bend) > np.pi:
            raise ValueError("positive shoot dimensions and bounded bend required")
        if not 0 <= self.first <= self.last <= 1 or not 0 < self.leaf_angle < np.pi:
            raise ValueError("valid parent interval and leaf angle required")
        if self.arrangement not in (
            "spiral",
            "distichous",
            "opposite",
            "decussate",
            "whorled",
        ):
            raise ValueError("declared phyllotaxis required")
        if type(self.whorl) is not int or not 1 <= self.whorl <= 16:
            raise ValueError("bounded positive whorl count required")
        if not isinstance(self.blade, Blade):
            raise TypeError("prepared blade recipe required")
        if self.normal_hint is not None:
            if not isinstance(self.normal_hint, tuple):
                raise ValueError("immutable normal hint required")
            hint = np.asarray(self.normal_hint, dtype=float)
            if (
                hint.shape != (3,)
                or not np.all(np.isfinite(hint))
                or not np.isfinite(np.linalg.norm(hint))
                or np.linalg.norm(hint) == 0
            ):
                raise ValueError("finite nonzero normal hint required")
        if not 0 <= self.normal_alignment < 1 or (
            self.normal_alignment and self.normal_hint is None
        ):
            raise ValueError("bounded normal alignment requires a direction hint")
        intervals = (self.last - self.first) * self.length / self.internode
        if not np.isfinite(intervals) or intervals >= (2**31 - 1) / self.per_group:
            raise ValueError("group count must fit the organ ID representation")

    @property
    def groups(self):
        return (
            int(np.floor((self.last - self.first) * self.length / self.internode)) + 1
        )

    @property
    def per_group(self):
        return (
            self.whorl
            if self.arrangement == "whorled"
            else 2
            if self.arrangement in ("opposite", "decussate")
            else 1
        )

    @property
    def count(self):
        return self.groups * self.per_group

    @cached_property
    def identity(self):
        data = json.dumps(
            {"version": "metamer-group-1", **asdict(self)}, sort_keys=True
        )
        return hashlib.sha256(data.encode()).hexdigest()

    def summary(self):
        return {
            "identity": self.identity,
            "leaves": self.count,
            "leaf_area_m2": self.count * self.blade.area(),
            "root_bound_radius_m": self.length
            + self.blade.length
            + self.blade.petiole
            + self.blade.width / 2 * np.sqrt(1 + self.blade.fold**2),
        }

    def detail(self, max_leaves):
        detail_budget(max_leaves)
        return self._placements if self.count <= max_leaves else None

    @cached_property
    def _placements(self):
        groups = np.repeat(np.arange(self.groups), self.per_group)
        member = np.tile(np.arange(self.per_group), self.groups)
        t = self.last - groups * self.internode / self.length
        centre, tangent, lateral, normal = arc_frame(self.length, self.bend, 0, t)
        increment = {"distichous": np.pi, "opposite": 0, "decussate": np.pi / 2}.get(
            self.arrangement, np.pi * (3 - np.sqrt(5))
        )
        phase = np.random.default_rng(self.seed).uniform(-np.pi, np.pi)
        angle = phase + groups * increment + member * 2 * np.pi / self.per_group
        radial = np.cos(angle)[:, None] * lateral + np.sin(angle)[:, None] * normal
        direction = np.cos(self.leaf_angle) * tangent + np.sin(self.leaf_angle) * radial
        if self.normal_alignment:
            hint = np.asarray(self.normal_hint, dtype=float)
            hint /= np.linalg.norm(hint)
            direction -= (
                self.normal_alignment
                * np.sum(direction * hint, axis=-1, keepdims=True)
                * hint
            )
            direction /= np.linalg.norm(direction, axis=-1, keepdims=True)
        frames = normal_frame(
            direction, tangent if self.normal_hint is None else self.normal_hint
        )
        origins = centre + self.blade.petiole * direction
        result = {
            "ids": np.arange(self.count, dtype=np.int32),
            "nodes": centre,
            "origins": origins,
            "frames": frames,
            "stem_parameter": t,
        }
        return MappingProxyType({key: immutable_array(v) for key, v in result.items()})
