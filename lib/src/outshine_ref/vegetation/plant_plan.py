"""Bounded branching recipes with canonical axes and deferred leaf groups."""

import hashlib
import itertools
import json
from dataclasses import asdict, dataclass, field
from functools import cached_property, lru_cache
from types import MappingProxyType

import numpy as np

from .blade_surface import Blade
from .curve_frame import arc_frame
from .plan_arrays import detail_budget, immutable_array
from .plant_levels import AxisLevel
from .shoot_plan import normal_frame

UP = immutable_array((0.0, 0.0, 1.0))
GOLDEN_ANGLE = np.pi * (3 - np.sqrt(5))
PLAN_VERSION = "branching-plant-2"


@lru_cache(maxsize=16)
def blade_areas(blades):
    return tuple(blade.area() for blade in blades)


@dataclass(frozen=True)
class AxisState:
    path: tuple[int, ...]
    id: int
    length: float
    bend: float
    origin: np.ndarray
    frame: np.ndarray
    phase: float
    parent_parameter: float

    def __post_init__(self):
        origin, frame = np.asarray(self.origin), np.asarray(self.frame)
        if (
            not isinstance(self.path, tuple)
            or len(self.path) > 7
            or any(type(slot) is not int or not 0 <= slot < 64 for slot in self.path)
            or type(self.id) is not int
            or not 0 <= self.id < 2**64
            or not np.isfinite(
                [self.length, self.bend, self.phase, self.parent_parameter]
            ).all()
            or self.length <= 0
            or abs(self.bend) > np.pi
            or abs(self.phase) > np.pi
            or not 0 <= self.parent_parameter <= 1
            or origin.dtype.kind not in "fiu"
            or frame.dtype.kind not in "fiu"
            or origin.shape != (3,)
            or frame.shape != (3, 3)
            or not np.isfinite(origin).all()
            or not np.isfinite(frame).all()
            or np.max(np.abs(frame.T @ frame - np.eye(3))) > 2e-14
            or abs(np.linalg.det(frame) - 1) > 2e-14
        ):
            raise ValueError(
                "finite position and right-handed orthonormal frame required"
            )
        object.__setattr__(
            self, "origin", immutable_array(origin.astype(np.float64, copy=False))
        )
        object.__setattr__(
            self, "frame", immutable_array(frame.astype(np.float64, copy=False))
        )


@dataclass(frozen=True)
class PlantPlan:
    seed: int
    levels: tuple[AxisLevel, ...]
    blades: tuple[Blade, ...]
    _axes: dict = field(default_factory=dict, init=False, repr=False, compare=False)
    _leaves: dict = field(default_factory=dict, init=False, repr=False, compare=False)

    def __post_init__(self):
        if type(self.seed) is not int or not 0 <= self.seed < 2**64:
            raise ValueError("unsigned 64-bit plant seed required")
        if not isinstance(self.levels, tuple) or not 1 <= len(self.levels) <= 8:
            raise ValueError("one to eight immutable axis orders required")
        if not all(isinstance(level, AxisLevel) for level in self.levels):
            raise ValueError("axis recipes required")
        if self.levels[0].branches != 1:
            raise ValueError("exactly one root axis required")
        if not isinstance(self.blades, tuple) or not 1 <= len(self.blades) <= 64:
            raise ValueError("one to 64 immutable blade variants required")
        if not all(isinstance(blade, Blade) for blade in self.blades):
            raise ValueError("blade recipes required")
        if len({blade.petiole for blade in self.blades}) != 1:
            raise ValueError("shared placement requires equal petiole lengths")
        for parent, child in zip(self.levels, self.levels[1:]):
            radius = (
                parent.base_radius
                + (parent.tip_radius - parent.base_radius) * child.last
            )
            if child.base_radius > radius:
                raise ValueError(
                    "child root must fit inside the parent at its attachment"
                )
        if sum(self.axis_counts) >= 2**64 or sum(self.leaf_counts) >= 2**64:
            raise ValueError("plant IDs must fit unsigned 64-bit integers")

    @cached_property
    def identity(self):
        return hashlib.sha256(
            json.dumps(self.recipe(), sort_keys=True).encode()
        ).hexdigest()

    def recipe(self):
        return {
            "version": PLAN_VERSION,
            "seed": self.seed,
            "levels": [asdict(level) for level in self.levels],
            "blades": [asdict(blade) for blade in self.blades],
        }

    @classmethod
    def from_recipe(cls, recipe):
        if (
            not isinstance(recipe, dict)
            or set(recipe) != {"version", "seed", "levels", "blades"}
            or recipe["version"] != PLAN_VERSION
        ):
            raise ValueError("complete supported plant recipe required")
        return cls(
            recipe["seed"],
            tuple(AxisLevel(**level) for level in recipe["levels"]),
            tuple(Blade(**blade) for blade in recipe["blades"]),
        )

    @cached_property
    def axis_counts(self):
        return tuple(
            itertools.accumulate(
                (level.branches for level in self.levels), lambda a, b: a * b
            )
        )

    @cached_property
    def slot_leaf_counts(self):
        return tuple(
            tuple(level.leaf_count(slot) for slot in range(level.branches))
            for level in self.levels
        )

    @cached_property
    def leaf_counts(self):
        return tuple(
            count // level.branches * sum(slots)
            for count, level, slots in zip(
                self.axis_counts, self.levels, self.slot_leaf_counts
            )
        )

    def ordinal(self, path):
        if not isinstance(path, tuple) or len(path) >= len(self.levels):
            raise ValueError("bounded immutable axis path required")
        ordinal = 0
        for depth, slot in enumerate(path, 1):
            if type(slot) is not int or not 0 <= slot < self.levels[depth].branches:
                raise ValueError("axis path outside the recipe")
            ordinal = ordinal * self.levels[depth].branches + slot
        return ordinal

    def leaf_prefix(self, depth, ordinal):
        slots = self.slot_leaf_counts[depth]
        cycles, remainder = divmod(ordinal, len(slots))
        return cycles * sum(slots) + sum(slots[:remainder])

    def summary(self, path=()):
        ordinal, depth = self.ordinal(path), len(path)
        variants, axes, leaves = [0] * len(self.blades), 0, 0
        for order in range(depth, len(self.levels)):
            count = self.axis_counts[order] // self.axis_counts[depth]
            offset = sum(self.leaf_counts[:order])
            first = offset + self.leaf_prefix(order, ordinal * count)
            last = offset + self.leaf_prefix(order, (ordinal + 1) * count)
            cycles, remainder = divmod(last - first, len(variants))
            variants = [value + cycles for value in variants]
            for index in range(remainder):
                variants[(first + self.seed + index) % len(variants)] += 1
            axes += count
            leaves += last - first
        extent = max(
            blade.length + blade.petiole + blade.width / 2 * np.sqrt(1 + blade.fold**2)
            for blade in self.blades
        )
        bound = self.levels[depth].length_at(path[-1] if path else 0)
        bound += sum(level.length for level in self.levels[depth + 1 :])
        bound += max(level.base_radius for level in self.levels[depth:]) + extent
        return {
            "identity": self.identity,
            "bound_anchor_path": path,
            "axes": axes,
            "leaves": leaves,
            "variant_counts": variants,
            "leaf_area_m2": sum(
                count * area for count, area in zip(variants, blade_areas(self.blades))
            ),
            "root_bound_radius_m": float(bound),
        }

    def children(self, path):
        self.ordinal(path)
        return (
            tuple(path + (slot,) for slot in range(self.levels[len(path) + 1].branches))
            if len(path) + 1 < len(self.levels)
            else ()
        )

    def paths(self):
        for depth in range(len(self.levels)):
            yield from itertools.product(
                *(range(level.branches) for level in self.levels[1 : depth + 1])
            )

    def axis(self, path):
        ordinal, depth = self.ordinal(path), len(path)
        if path in self._axes:
            return self._axes[path]
        level = self.levels[depth]
        identity = sum(self.axis_counts[:depth]) + ordinal
        rng = np.random.default_rng(np.random.SeedSequence((self.seed, identity)))
        phase = rng.uniform(-np.pi, np.pi)
        bend = level.bend + rng.uniform(-level.bend_variation, level.bend_variation)
        if path:
            parent = self.axis(path[:-1])
            jitter = rng.uniform(-1, 1) if level.attachment_jitter else 0.0
            parameter = level.attachment(path[-1], jitter)
            centre, tangent, lateral, normal = arc_frame(
                parent.length, parent.bend, 0, parameter
            )
            angle = (
                parent.phase
                + path[-1] * GOLDEN_ANGLE
                + rng.uniform(-level.azimuth_variation, level.azimuth_variation)
            )
            radial = np.cos(angle) * lateral + np.sin(angle) * normal
            inclination = level.angle + rng.uniform(
                -level.angle_variation, level.angle_variation
            )
            direction = parent.frame @ (
                np.cos(inclination) * tangent + np.sin(inclination) * radial
            )
            if level.upward_bias:
                direction = (1 - level.upward_bias) * direction + level.upward_bias * UP
                direction /= np.linalg.norm(direction)
            origin, frame = (
                parent.origin + parent.frame @ centre,
                normal_frame(direction, UP),
            )
        else:
            parameter, origin = 0.0, np.zeros(3)
            frame = normal_frame(UP, np.array((np.cos(phase), np.sin(phase), 0)))
        state = AxisState(
            path,
            identity,
            level.length_at(path[-1] if path else 0),
            bend,
            origin,
            frame,
            phase,
            parameter,
        )
        self._axes[path] = state
        return state

    def leaves(self, path, max_leaves):
        ordinal, depth = self.ordinal(path), len(path)
        detail_budget(max_leaves)
        level, slot = self.levels[depth], path[-1] if path else 0
        count = self.slot_leaf_counts[depth][slot]
        if count == 0 or count > max_leaves:
            return None
        if path in self._leaves:
            return self._leaves[path]
        axis = self.axis(path)
        seed = (self.seed ^ ((axis.id + 1) * 0x9E3779B97F4A7C15)) % 2**64
        shoot = level.shoot(
            slot, seed, axis.bend, self.blades[0], tuple(axis.frame.T @ UP)
        )
        local = shoot.detail(count)
        first = sum(self.leaf_counts[:depth]) + self.leaf_prefix(depth, ordinal)
        ids = np.arange(first, first + count, dtype=np.uint64)
        result = {
            "ids": ids,
            "variants": (
                (ids % len(self.blades) + self.seed % len(self.blades))
                % len(self.blades)
            ).astype(np.uint8),
            "nodes": local["nodes"] @ axis.frame.T + axis.origin,
            "origins": local["origins"] @ axis.frame.T + axis.origin,
            "frames": np.einsum("ij,njk->nik", axis.frame, local["frames"]),
            "stem_parameter": local["stem_parameter"],
        }
        self._leaves[path] = MappingProxyType(
            {key: immutable_array(v) for key, v in result.items()}
        )
        return self._leaves[path]

    def expand(self, max_axes, max_leaves):
        detail_budget(max_axes)
        detail_budget(max_leaves)
        if sum(self.axis_counts) > max_axes or sum(self.leaf_counts) > max_leaves:
            return None
        axes, groups = [], []
        for path in self.paths():
            axes.append(self.axis(path))
            group = self.leaves(path, max_leaves)
            if group is not None:
                groups.append(group)
        return {"axes": tuple(axes), "groups": tuple(groups)}

    def tables(self, max_axes, max_leaves):
        detail_budget(max_axes)
        detail_budget(max_leaves)
        if sum(self.axis_counts) > max_axes or sum(self.leaf_counts) > max_leaves:
            return None
        return self._tables

    @cached_property
    def _tables(self):
        from .plant_tables import canonical_tables

        return canonical_tables(self)
