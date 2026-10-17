"""Bounded physical internodes and whorls; complete sites before organ meshing."""

from dataclasses import dataclass, field

import numpy as np

from .profiles import Profile

MAX_SITES = 4096
EMPTY_SITES = np.frombuffer(b"", dtype=np.float64).reshape(0, 2)


@dataclass(frozen=True)
class DistancePattern:
    mode: str = "phyllotaxy"
    spacing: Profile = field(default_factory=lambda: Profile(np.full(4, 0.08)))
    whorl_size: int = 1
    divergence: float = np.pi * (3 - np.sqrt(5))
    spread_m: float = 0.0
    limit_groups: int = 4096

    def __post_init__(self):
        if self.mode not in ("phyllotaxy", "interval", "uniform"):
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
        if self.mode == "uniform" and (
            self.spread_m or not np.all(self.spacing.table == self.spacing.table[0])
        ):
            raise ValueError(
                "uniform groups require constant minimum spacing without spread"
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

    def _capacities(self, lengths, first, last):
        if (
            not np.all(np.isfinite(lengths))
            or np.any(lengths < 0)
            or not 0 <= first <= last <= 1
        ):
            raise ValueError(
                "finite nonnegative parent lengths and ordered interval required"
            )
        if first == last or self.limit_groups == 0:
            return np.zeros(len(lengths), dtype=np.int64)
        with np.errstate(over="ignore"):
            ratio = (last - first) * lengths / float(self.spacing.table.min())
        ratio = np.minimum(ratio, self.limit_groups)
        tolerance = 32 * np.finfo(float).eps * np.maximum(1, ratio)
        groups = np.minimum(
            self.limit_groups, np.floor(ratio + tolerance).astype(np.int64) + 1
        )
        capacities = np.where(lengths == 0, 0, groups * self.whorl_size)
        if np.any(capacities > MAX_SITES):
            raise ValueError("complete parent placement exceeds the site budget")
        return capacities

    def tables(self, lengths, first, last, max_sites=1_000_000, seeds=None):
        lengths = np.asarray(lengths, dtype=float)
        if lengths.ndim != 1 or len(lengths) > 65536:
            raise ValueError("bounded one-dimensional parent lengths required")
        if type(max_sites) is not int or max_sites < 0:
            raise ValueError("nonnegative complete site budget required")
        capacities = self._capacities(lengths, first, last)
        if int(capacities.sum()) > max_sites:
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
            if self.mode == "uniform":
                fraction = number / np.maximum(groups[parent] - 1, 1)
                values = last - fraction * (last - first)
            else:
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
        group_max = int(groups.max(initial=0))
        angles = np.repeat(np.arange(group_max) * self.divergence, self.whorl_size)
        angles += np.tile(
            np.arange(self.whorl_size) * (2 * np.pi / self.whorl_size), group_max
        )
        result = [EMPTY_SITES] * len(lengths)
        occupied = np.flatnonzero(groups)
        for index, begin, end in zip(
            occupied, offsets[occupied], offsets[occupied + 1], strict=True
        ):
            row = positions[begin:end]
            row = row[np.isfinite(row)]
            if not len(row):
                continue
            if np.any(np.diff(row) >= 0):
                raise ValueError(
                    "distinct groups cannot be resolved at this parent scale"
                )
            parameter = np.repeat(row, self.whorl_size)
            if self.spread_m and len(row):
                rng = np.random.default_rng(0 if seeds is None else seeds[index])
                parameter += (
                    rng.uniform(-self.spread_m, self.spread_m, len(parameter))
                    / lengths[index]
                )
                parameter = np.clip(parameter, first, last)
            values = np.column_stack((parameter, angles[: len(parameter)]))
            result[index] = np.frombuffer(values.tobytes(), dtype=np.float64).reshape(
                -1, 2
            )
        return tuple(result)
