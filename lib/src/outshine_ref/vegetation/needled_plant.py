"""A stable wood plan with deferred physical needles, independent of parent scale."""

import hashlib
import json
from dataclasses import dataclass, field
from functools import cached_property
from types import MappingProxyType

import numpy as np

from .needle_pattern import NeedlePattern
from .needle_surface import Needle
from .plan_arrays import detail_budget, immutable_array, repeated_prefix
from .plant_plan import PlantPlan


@dataclass(frozen=True)
class NeedledPlant:
    wood: PlantPlan
    patterns: tuple[NeedlePattern | None, ...]
    needles: tuple[Needle, ...]
    _groups: dict = field(default_factory=dict, init=False, repr=False, compare=False)

    def __post_init__(self):
        if not isinstance(self.wood, PlantPlan) or self.wood.blades:
            raise ValueError("bladeless wood plan required")
        if (
            not isinstance(self.patterns, tuple)
            or len(self.patterns) != len(self.wood.levels)
            or not any(isinstance(p, NeedlePattern) for p in self.patterns)
            or not all(p is None or isinstance(p, NeedlePattern) for p in self.patterns)
        ):
            raise ValueError("one optional needle pattern per wood order required")
        if (
            not isinstance(self.needles, tuple)
            or not 1 <= len(self.needles) <= 16
            or not all(isinstance(n, Needle) for n in self.needles)
        ):
            raise ValueError("one to 16 physical needle variants required")
        if sum(self.needle_counts) >= 2**64:
            raise ValueError("needle identities must fit UInt64")
        for level, p in zip(self.wood.levels, self.patterns):
            if (
                p
                and min(level.length_at(slot) for slot in range(level.branches))
                < 8 * level.base_radius
            ):
                raise ValueError("needle-bearing twigs require slender physical radii")

    @cached_property
    def slot_counts(self):
        return tuple(
            tuple(
                p.count(level.length_at(slot)) if p else 0
                for slot in range(level.branches)
            )
            for level, p in zip(self.wood.levels, self.patterns)
        )

    @cached_property
    def needle_counts(self):
        return tuple(
            axes // level.branches * sum(slots)
            for axes, level, slots in zip(
                self.wood.axis_counts, self.wood.levels, self.slot_counts
            )
        )

    @cached_property
    def group_counts(self):
        return tuple(
            count // p.group_size if p else 0
            for count, p in zip(self.needle_counts, self.patterns)
        )

    @cached_property
    def identity(self):
        return hashlib.sha256(
            json.dumps(self.recipe(), sort_keys=True).encode()
        ).hexdigest()

    def recipe(self):
        return {
            "version": "needled-plant-1",
            "wood": self.wood.recipe(),
            "patterns": [p.recipe() if p else None for p in self.patterns],
            "needles": [n.recipe() for n in self.needles],
        }

    @classmethod
    def from_recipe(cls, recipe):
        if (
            not isinstance(recipe, dict)
            or set(recipe) != {"version", "wood", "patterns", "needles"}
            or recipe["version"] != "needled-plant-1"
        ):
            raise ValueError("complete supported needled plant recipe required")
        for needle in recipe["needles"]:
            keys = set(Needle.__dataclass_fields__)
            if isinstance(needle, dict) and needle.get("section") != "sector":
                keys.remove("sector_count")
            if not isinstance(needle, dict) or set(needle) != keys:
                raise ValueError("complete needle shape recipes required")
        return cls(
            PlantPlan.from_recipe(recipe["wood"]),
            tuple(
                NeedlePattern.from_recipe(p) if p is not None else None
                for p in recipe["patterns"]
            ),
            tuple(Needle(**n) for n in recipe["needles"]),
        )

    def summary(self, path=()):
        ordinal, depth = self.wood.ordinal(path), len(path)
        count, groups = 0, 0
        for order in range(depth, len(self.patterns)):
            axes = self.wood.axis_counts[order] // self.wood.axis_counts[depth]
            slots, p = self.slot_counts[order], self.patterns[order]
            local = repeated_prefix(slots, (ordinal + 1) * axes) - repeated_prefix(
                slots, ordinal * axes
            )
            count += local
            groups += local // p.group_size if p else 0
        base = self.wood.summary(path)
        reach = max(n.length + n.radius for n in self.needles)
        reach *= 1 + max(p.scale_variation for p in self.patterns if p)
        reach += max(p.short_length for p in self.patterns if p)
        return {
            "identity": self.identity,
            "bound_anchor_path": path,
            "axes": base["axes"],
            "needles": count,
            "groups": groups,
            "root_bound_radius_m": base["root_bound_radius_m"] + reach,
        }

    def foliage(self, path, max_needles, *, retain=True):
        """Return owned physical poses; optionally leave new details unretained."""
        if type(retain) is not bool:
            raise ValueError("explicit boolean detail retention required")
        ordinal, depth = self.wood.ordinal(path), len(path)
        detail_budget(max_needles)
        slot, p = path[-1] if path else 0, self.patterns[depth]
        count = self.slot_counts[depth][slot]
        if count == 0 or count > max_needles:
            return None
        if path in self._groups:
            return self._groups[path]
        axis, level = self.wood.axis(path), self.wood.levels[depth]
        local = p.detail(
            axis, (level.base_radius, level.tip_radius), self.wood.seed, count
        )
        first = sum(self.needle_counts[:depth]) + repeated_prefix(
            self.slot_counts[depth], ordinal
        )
        group_first = (
            sum(self.group_counts[:depth])
            + repeated_prefix(self.slot_counts[depth], ordinal) // p.group_size
        )
        ids = np.arange(first, first + count, dtype=np.uint64)
        changes = {
            "ids": ids,
            "group_ids": local["group_ids"].astype(np.uint64) + group_first,
            "short_ids": np.arange(
                group_first, group_first + count // p.group_size, dtype=np.uint64
            ),
            "axis_ids": np.full(count, axis.id, dtype=np.uint64),
            "short_axis_ids": np.full(count // p.group_size, axis.id, dtype=np.uint64),
            "variants": (
                (ids % len(self.needles) + self.wood.seed % len(self.needles))
                % len(self.needles)
            ).astype(np.uint8),
        }
        result = MappingProxyType(
            dict(local) | {k: immutable_array(v) for k, v in changes.items()}
        )
        if retain:
            self._groups[path] = result
        return result

    def tables(self, max_axes, max_needles):
        detail_budget(max_axes)
        detail_budget(max_needles)
        if (
            sum(self.wood.axis_counts) > max_axes
            or sum(self.needle_counts) > max_needles
        ):
            return None
        return self._tables

    @cached_property
    def _tables(self):
        from .needle_tables import canonical_tables

        return canonical_tables(self)
