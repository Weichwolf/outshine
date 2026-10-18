"""Explicit inclination profiles and world-up bias over parent-dependent growth."""

from dataclasses import dataclass, replace
from functools import cached_property
from types import MappingProxyType

import numpy as np
from generator_specs import profile_from_recipe, profile_recipe
from outshine_ref.vegetation import Profile
from outshine_ref.vegetation.shoot_plan import normal_frame
from parent_growth_graph import ParentGrowthGraph


def bindings(graph, pairs, profiles):
    if not isinstance(pairs, tuple) or len(pairs) > len(graph.nodes):
        raise ValueError("bounded immutable generator bindings required")
    result = []
    for pair in pairs:
        if not isinstance(pair, tuple) or len(pair) != 2 or type(pair[0]) is not int:
            raise ValueError("generator/value pairs required")
        key, value = pair
        if key not in graph.by_id or graph.by_id[key].kind != "axis":
            raise ValueError("inclination binding requires a physical axis")
        if profiles:
            variance = graph.by_id[key].placement.pitch_variance
            if (
                not isinstance(value, Profile)
                or value.table.min() <= variance
                or value.table.max() >= np.pi - variance
            ):
                raise ValueError(
                    "inclination and its variation must stay within zero/pi"
                )
        else:
            raw = np.asarray(value)
            if raw.shape != () or raw.dtype.kind not in "fiu" or not np.isfinite(raw):
                raise ValueError("finite scalar upward bias required")
            value = float(raw)
            if not 0 <= value < 0.5:
                raise ValueError("upward bias must lie in [0, 0.5)")
        result.append((key, value))
    if len({key for key, _ in result}) != len(result):
        raise ValueError("one binding per generator required")
    return tuple(sorted(result))


@dataclass(frozen=True)
class ParentProfileGraph(ParentGrowthGraph):
    inclination_profiles: tuple[tuple[int, Profile], ...] = ()
    upward_biases: tuple[tuple[int, float], ...] = ()

    def __post_init__(self):
        super().__post_init__()
        object.__setattr__(
            self,
            "inclination_profiles",
            bindings(self, self.inclination_profiles, True),
        )
        object.__setattr__(
            self, "upward_biases", bindings(self, self.upward_biases, False)
        )
        if not self.inclination_profiles and not self.upward_biases:
            raise ValueError("explicit inclination or upward binding required")

    @cached_property
    def inclinations(self):
        return MappingProxyType(dict(self.inclination_profiles))

    @cached_property
    def biases(self):
        return MappingProxyType(dict(self.upward_biases))

    def _place(self, path, parent, site):
        pose = super()._place(path, parent, site)
        key = path[-1][0]
        if key not in self.inclinations and not self.biases.get(key, 0):
            return pose
        frame = pose.frame
        if key in self.inclinations:
            angle = (
                float(self.inclinations[key].evaluate(site[0]))
                - self.by_id[key].placement.pitch
            )
            c, s = np.cos(angle), np.sin(angle)
            frame = frame @ np.array(((1, 0, 0), (0, c, -s), (0, s, c)))
        if self.biases.get(key, 0):
            bias = self.biases[key]
            direction = (1 - bias) * frame[:, 1] + bias * np.array((0, 0, 1))
            frame = normal_frame(direction, frame[:, 2])
        return replace(pose, frame=frame)

    def recipe(self):
        return super().recipe() | {
            "version": "parent-profile-generator-graph-1",
            "inclinations": [
                [key, profile_recipe(profile)]
                for key, profile in self.inclination_profiles
            ],
            "upward_biases": [list(pair) for pair in self.upward_biases],
        }

    @classmethod
    def from_recipe(cls, recipe):
        if (
            not isinstance(recipe, dict)
            or set(recipe)
            != {
                "version",
                "seed",
                "nodes",
                "parent_length_ratios",
                "inclinations",
                "upward_biases",
            }
            or recipe["version"] != "parent-profile-generator-graph-1"
        ):
            raise ValueError("complete explicit parent-profile graph recipe required")
        parent = ParentGrowthGraph.from_recipe(
            {
                key: value
                for key, value in recipe.items()
                if key not in ("inclinations", "upward_biases")
            }
            | {"version": "parent-generator-graph-1"}
        )
        try:
            inclinations = tuple(
                (key, profile_from_recipe(profile))
                for key, profile in recipe["inclinations"]
            )
            biases = tuple(tuple(pair) for pair in recipe["upward_biases"])
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError("complete parent-profile bindings required") from error
        return cls(
            parent.seed,
            parent.nodes,
            parent_length_ratios=parent.parent_length_ratios,
            inclination_profiles=inclinations,
            upward_biases=biases,
        )
