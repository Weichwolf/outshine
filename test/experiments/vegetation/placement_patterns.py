"""Bounded physical internodes and whorls; complete sites before organ meshing."""

from dataclasses import dataclass, field
from itertools import pairwise

import numpy as np
from profile_curve import Profile

MAX_SITES = 4096
EMPTY_SITES = np.empty((0, 2))
EMPTY_SITES.setflags(write=False)


@dataclass(frozen=True)
class DistancePattern:
    mode: str = "phyllotaxy"
    spacing: Profile = field(default_factory=lambda: Profile(np.full(4, 0.08)))
    whorl_size: int = 1
    divergence: float = np.pi * (3 - np.sqrt(5))
    spread_m: float = 0.0
    limit_groups: int = 4096

    def __post_init__(self):
        if self.mode not in ("phyllotaxy", "interval"):
            raise ValueError("declared distance generation mode required")
        if not isinstance(self.spacing, Profile) or self.spacing.table.min() <= 0:
            raise ValueError("positive prepared physical internode profile required")
        if type(self.whorl_size) is not int or not 1 <= self.whorl_size <= 64:
            raise ValueError("one to 64 organs per whorl required")
        if type(self.limit_groups) is not int or not 0 <= self.limit_groups <= 4096:
            raise ValueError("zero to 4096 explicitly limited groups required")
        if (
            not np.isfinite(self.divergence)
            or abs(self.divergence) > 2 * np.pi
            or not np.isfinite(self.spread_m)
            or self.spread_m < 0
        ):
            raise ValueError(
                "finite divergence and nonnegative physical spread required"
            )

    def recipe(self):
        return {
            "mode": self.mode,
            "spacing": {
                "kind": self.spacing.kind,
                "table": self.spacing.table.tolist(),
            },
            "whorl_size": self.whorl_size,
            "divergence": self.divergence,
            "spread_m": self.spread_m,
            "limit_groups": self.limit_groups,
        }

    @classmethod
    def from_recipe(cls, recipe):
        values = dict(recipe)
        profile = values["spacing"]
        values["spacing"] = Profile(np.asarray(profile["table"]), profile["kind"])
        return cls(**values)

    def capacity(self, length, first, last):
        if not np.isfinite(length) or length < 0 or not 0 <= first <= last <= 1:
            raise ValueError(
                "finite nonnegative parent length and ordered interval required"
            )
        if length == 0 or first == last or self.limit_groups == 0:
            return 0
        ratio = (last - first) * float(length) / float(self.spacing.table.min())
        groups = (
            self.limit_groups
            if ratio >= self.limit_groups
            else min(
                self.limit_groups,
                int(np.floor(ratio + 32 * np.finfo(float).eps * max(1, ratio))) + 1,
            )
        )
        count = groups * self.whorl_size
        if count > MAX_SITES:
            raise ValueError("complete parent placement exceeds the site budget")
        return count

    def tables(self, lengths, first, last, max_sites=1_000_000, seeds=None):
        lengths = np.asarray(lengths, dtype=float)
        if lengths.ndim != 1 or len(lengths) > 65536:
            raise ValueError("bounded one-dimensional parent lengths required")
        if type(max_sites) is not int or max_sites < 0:
            raise ValueError("nonnegative complete site budget required")
        capacities = np.array(
            [self.capacity(v, first, last) for v in lengths], dtype=np.int64
        )
        if sum(map(int, capacities)) > max_sites:
            return None
        if seeds is not None and len(seeds) != len(lengths):
            raise ValueError("one independent seed per parent required")
        groups = capacities // self.whorl_size
        offsets = np.r_[0, np.cumsum(groups)]
        positions = np.full(int(offsets[-1]), np.nan)
        tolerance = 32 * np.finfo(float).eps
        if np.all(self.spacing.table == self.spacing.table[0]):
            parent = np.repeat(np.arange(len(lengths)), groups)
            number = np.arange(len(positions)) - np.repeat(offsets[:-1], groups)
            values = last - number * (self.spacing.table[0] / lengths[parent])
            valid = values >= first - tolerance
            positions[valid] = np.maximum(values[valid], first)
        else:
            current = np.full(len(lengths), last, dtype=float)
            active = np.flatnonzero(groups)
            for index in range(int(groups.max(initial=0))):
                active = active[
                    (current[active] >= first - tolerance) & (index < groups[active])
                ]
                if not len(active):
                    break
                positions[offsets[active] + index] = np.maximum(current[active], first)
                step = self.spacing.evaluate(current[active]) / lengths[active]
                following = current[active] - step
                if np.any(following >= current[active]):
                    raise ValueError(
                        "internode step cannot advance at this parent scale"
                    )
                current[active] = following
        result = []
        for index, (begin, end) in enumerate(pairwise(offsets)):
            row = positions[begin:end]
            row = row[np.isfinite(row)]
            if not len(row):
                result.append(EMPTY_SITES)
                continue
            if np.any(np.diff(row) >= 0):
                raise ValueError(
                    "distinct groups cannot be resolved at this parent scale"
                )
            parameter = np.repeat(row, self.whorl_size)
            angle = np.repeat(np.arange(len(row)) * self.divergence, self.whorl_size)
            angle += np.tile(
                np.arange(self.whorl_size) * (2 * np.pi / self.whorl_size), len(row)
            )
            if self.spread_m and len(row):
                rng = np.random.default_rng(0 if seeds is None else seeds[index])
                parameter += (
                    rng.uniform(-self.spread_m, self.spread_m, len(parameter))
                    / lengths[index]
                )
                parameter = np.clip(parameter, first, last)
            table = np.column_stack((parameter, angle))
            table.setflags(write=False)
            result.append(table)
        return tuple(result)
