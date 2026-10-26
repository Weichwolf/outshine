"""Experimental parent-length binding over the existing generator graph."""

import math
from dataclasses import dataclass, replace
from functools import cached_property
from types import MappingProxyType

import numpy as np
from generator_graph import GeneratorGraph, shape_bound
from generator_specs import Generator


def positive_ratio(value):
    raw = np.asarray(value)
    if raw.shape != () or raw.dtype.kind not in "fiu" or not np.isfinite(raw):
        raise ValueError("finite positive scalar parent-length ratio required")
    result = float(raw)
    if not math.isfinite(result) or result <= 0:
        raise ValueError("parent-length ratio must be positive representable Float64")
    return result


def relative_scale(local_scale, parent_length, ratio, prototype_length):
    length = float(parent_length) * ratio * float(local_scale)
    scale = length / float(prototype_length)
    if not math.isfinite(scale) or scale <= 0 or not math.isfinite(length):
        raise ValueError("finite positive inherited length and instance scale required")
    return scale


def check_radius_binding(node, parent, ratio):
    placement = node.placement
    if placement.pattern is None:
        positions = np.array([placement.parameter(i) for i in range(placement.count)])
    else:
        knots = np.r_[
            placement.first,
            placement.last,
            np.linspace(0, 1, len(placement.scale.table)),
            np.linspace(0, 1, len(parent.shape.radius.table)),
        ]
        positions = np.unique(
            knots[(knots >= placement.first) & (knots <= placement.last)]
        )
    if not len(positions):
        return
    child_base = float(node.shape.radius.evaluate(0)) * float(
        max(getattr(node.shape, "ellipse", (1, 1)))
    )
    factor = child_base * float(parent.shape.length) * ratio / float(node.shape.length)
    factor *= 1 + placement.scale_variance
    maximum = factor * float(placement.scale.table.max())
    if not math.isfinite(maximum) or factor <= 0:
        raise ValueError("finite positive inherited axis radius required")
    child = factor * placement.scale.evaluate(positions)
    available = parent.shape.radius.evaluate(positions) * float(
        min(getattr(parent.shape, "ellipse", (1, 1)))
    )
    if np.any(child > available):
        raise ValueError(
            "inherited child base must fit every possible parent attachment"
        )


@dataclass(frozen=True)
class ParentGrowthGraph(GeneratorGraph):
    parent_length_ratios: tuple[tuple[int, float], ...] = ()

    def __post_init__(self):
        if (
            not isinstance(self.nodes, tuple)
            or not 1 <= len(self.nodes) <= 64
            or not all(isinstance(node, Generator) for node in self.nodes)
        ):
            raise ValueError("immutable implemented generator nodes required")
        if not isinstance(self.parent_length_ratios, tuple) or not 1 <= len(
            self.parent_length_ratios
        ) <= len(self.nodes):
            raise ValueError("explicit immutable parent-length bindings required")
        pairs = []
        for pair in self.parent_length_ratios:
            if (
                not isinstance(pair, tuple)
                or len(pair) != 2
                or type(pair[0]) is not int
            ):
                raise ValueError("generator ID and scalar length ratio pairs required")
            pairs.append((pair[0], positive_ratio(pair[1])))
        if len({key for key, _ in pairs}) != len(pairs):
            raise ValueError("one parent-length binding per generator required")
        object.__setattr__(self, "parent_length_ratios", tuple(sorted(pairs)))
        nodes = {node.id: node for node in self.nodes}
        for key, ratio in pairs:
            node = nodes.get(key)
            if (
                node is None
                or node.kind != "axis"
                or not node.parents
                or any(p not in nodes or nodes[p].kind != "axis" for p in node.parents)
            ):
                raise ValueError(
                    "parent-length binding needs an axis and physical parents"
                )
            for parent in node.parents:
                check_radius_binding(node, nodes[parent], ratio)
        super().__post_init__()

    @cached_property
    def ratios(self):
        return MappingProxyType(dict(self.parent_length_ratios))

    def maximum_scale(self, node, parent_lengths):
        local = float(node.placement.scale.table.max()) * (
            1 + node.placement.scale_variance
        )
        if node.id not in self.ratios:
            return local
        parent = max(parent_lengths[p] for p in node.parents)
        return relative_scale(local, parent, self.ratios[node.id], node.shape.length)

    @cached_property
    def length_bounds(self):
        lengths = {}
        for key in self.order:
            node = self.by_id[key]
            lengths[key] = (
                float(node.shape.length) * self.maximum_scale(node, lengths)
                if node.shape is not None
                else 0.0
            )
            if not math.isfinite(lengths[key]):
                raise ValueError("finite complete parent-length bounds required")
        return MappingProxyType({node.id: lengths[node.id] for node in self.nodes})

    @cached_property
    def bounds(self):
        result = {}
        for key in self.order:
            node = self.by_id[key]
            if self.capacities[key] == 0:
                result[key] = 0.0
                continue
            scale = self.maximum_scale(node, self.length_bounds)
            result[key] = max((result[p] for p in node.parents), default=0.0)
            result[key] += scale * float(shape_bound(node.shape))
            if not math.isfinite(result[key]):
                raise ValueError("finite complete parent-growth bound required")
        return MappingProxyType(result)

    def _place(self, path, parent, site):
        pose = super()._place(path, parent, site)
        key = path[-1][0]
        if key not in self.ratios:
            return pose
        node = self.by_id[key]
        scale = relative_scale(
            pose.scale, parent.length, self.ratios[key], node.shape.length
        )
        return replace(pose, scale=scale, length=node.shape.length * scale)

    def recipe(self):
        return super().recipe() | {
            "version": "parent-generator-graph-1",
            "parent_length_ratios": [list(pair) for pair in self.parent_length_ratios],
        }

    @classmethod
    def from_recipe(cls, recipe):
        if (
            not isinstance(recipe, dict)
            or set(recipe) != {"version", "seed", "nodes", "parent_length_ratios"}
            or recipe["version"] != "parent-generator-graph-1"
        ):
            raise ValueError("complete explicit parent-growth graph recipe required")
        try:
            nodes = tuple(Generator.from_recipe(node) for node in recipe["nodes"])
            pairs = tuple(tuple(pair) for pair in recipe["parent_length_ratios"])
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError("complete parent-growth graph fields required") from error
        return cls(recipe["seed"], nodes, parent_length_ratios=pairs)
