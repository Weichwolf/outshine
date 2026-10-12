"""Canonical generator topology, bounded instance requests and stable hierarchy paths."""

import hashlib
import heapq
import json
from dataclasses import dataclass, field
from functools import cached_property
from types import MappingProxyType

import numpy as np
from curve_frame import arc_frame
from generator_specs import FrondShape, Generator, TubeShape

UPRIGHT = np.array(((1, 0, 0), (0, 0, -1), (0, 1, 0)), dtype=float)
MAX_POSES = 100_000


def shape_bound(shape):
    if shape is None:
        return 0.0
    if isinstance(shape, TubeShape):
        return shape.length + shape.radius.table.max()
    if isinstance(shape, FrondShape):
        p, g = shape.pattern, shape.guide
        x = max(p.leaflet_length * (1 + p.variance), p.length * (1 - p.last)) * (
            1 + p.aspect / 2
        )
        q = max(x, 0.004) * g.width.table.max()
        d = 0.004 * g.height.table.max() + abs(g.fold_per_m) * q**2
        return g.length + np.hypot(q, d)
    return shape.length + shape.petiole + shape.width * (1 + abs(shape.fold))


@dataclass(frozen=True)
class Instance:
    path: tuple[tuple[int, int], ...]
    origin: np.ndarray
    frame: np.ndarray
    scale: float
    length: float
    bend: float
    anchor: np.ndarray

    def __post_init__(self):
        for name in ("origin", "frame", "anchor"):
            value = np.array(getattr(self, name), dtype=float, copy=True)
            value.setflags(write=False)
            object.__setattr__(self, name, value)


@dataclass(frozen=True)
class GeneratorGraph:
    seed: int
    nodes: tuple[Generator, ...]
    _poses: dict = field(default_factory=dict, init=False, repr=False, compare=False)
    _pose_counts: dict = field(
        default_factory=dict, init=False, repr=False, compare=False
    )

    def __post_init__(self):
        if (
            type(self.seed) is not int
            or not 0 <= self.seed < 2**64
            or not isinstance(self.nodes, tuple)
            or not 1 <= len(self.nodes) <= 64
        ):
            raise ValueError("stable seed and one to 64 immutable generators required")
        if not all(isinstance(n, Generator) for n in self.nodes) or len(
            {n.id for n in self.nodes}
        ) != len(self.nodes):
            raise ValueError("distinct implemented generator recipes required")
        if (
            sum(n.kind == "root" for n in self.nodes) != 1
            or sum(len(n.parents) for n in self.nodes) > 256
        ):
            raise ValueError("one root and bounded generator connections required")
        if any(p not in self.by_id for n in self.nodes for p in n.parents):
            raise ValueError("every generator parent must be present")
        if sum(self.counts.values()) >= 2**64:
            raise ValueError("bounded logical instance count required")
        if not all(np.isfinite(radius) for radius in self.bounds.values()):
            raise ValueError("finite complete graph bounds required")

    @cached_property
    def by_id(self):
        return MappingProxyType({n.id: n for n in self.nodes})

    @cached_property
    def order(self):
        incoming = {n.id: len(n.parents) for n in self.nodes}
        children = {n.id: [] for n in self.nodes}
        for n in self.nodes:
            for parent in n.parents:
                children[parent].append(n.id)
        ready = [key for key, count in incoming.items() if count == 0]
        heapq.heapify(ready)
        result = []
        while ready:
            key = heapq.heappop(ready)
            result.append(key)
            for child in children[key]:
                incoming[child] -= 1
                if incoming[child] == 0:
                    heapq.heappush(ready, child)
        if len(result) != len(self.nodes):
            raise ValueError("acyclic generator connections required")
        return tuple(result)

    @cached_property
    def counts(self):
        result = {}
        for key in self.order:
            node = self.by_id[key]
            result[key] = (
                node.placement.count * sum(result[p] for p in node.parents)
                if node.parents
                else 1
            )
        return MappingProxyType(result)

    @cached_property
    def bounds(self):
        result = {}
        for key in self.order:
            node = self.by_id[key]
            if self.counts[key] == 0:
                result[key] = 0.0
                continue
            scale = node.placement.scale.table.max() * (
                1 + node.placement.scale_variance
            )
            result[key] = max(
                (result[p] for p in node.parents), default=0.0
            ) + scale * shape_bound(node.shape)
            if not np.isfinite(result[key]):
                raise ValueError("finite complete graph bound required")
        return MappingProxyType(result)

    def recipe(self):
        return {
            "version": "generator-graph-1",
            "seed": self.seed,
            "nodes": [self.by_id[key].recipe() for key in sorted(self.by_id)],
        }

    @classmethod
    def from_recipe(cls, recipe):
        if recipe["version"] != "generator-graph-1":
            raise ValueError("supported complete generator graph required")
        return cls(
            recipe["seed"], tuple(Generator.from_recipe(n) for n in recipe["nodes"])
        )

    @cached_property
    def identity(self):
        recipe = self.recipe()
        recipe["nodes"] = [
            {key: value for key, value in node.items() if key != "label"}
            for node in recipe["nodes"]
        ]
        return hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()

    def summary(self):
        return {
            "identity": self.identity,
            "instances": sum(self.counts.values()),
            "counts": dict(self.counts),
            "bound_radius_m": max(self.bounds.values()),
        }

    def paths(self, node_id):
        if type(node_id) is not int or node_id not in self.by_id:
            raise ValueError("known generator ID required")
        node = self.by_id[node_id]
        if node.kind == "root":
            yield ((node_id, 0),)
        else:
            for parent in sorted(node.parents):
                for path in self.paths(parent):
                    for slot in range(node.placement.count):
                        yield (*path, (node_id, slot))

    def instance(self, path):
        if not isinstance(path, tuple) or not 1 <= len(path) <= len(self.nodes):
            raise ValueError("bounded immutable hierarchy path required")
        for index, entry in enumerate(path):
            if not isinstance(entry, tuple) or len(entry) != 2:
                raise ValueError("generator/slot pairs required")
            key, slot = entry
            if (
                type(key) is not int
                or key not in self.by_id
                or type(slot) is not int
                or not 0 <= slot < self.by_id[key].placement.count
            ):
                raise ValueError("known generator and valid slot required")
            node = self.by_id[key]
            if (
                index == 0
                and node.kind != "root"
                or index
                and path[index - 1][0] not in node.parents
            ):
                raise ValueError(
                    "path must follow root and declared generator connections"
                )
        if path in self._poses:
            return self._poses[path]
        missing = sum(path[:end] not in self._poses for end in range(1, len(path) + 1))
        if len(self._poses) + missing > MAX_POSES:
            raise ValueError("materialized hierarchy pose budget exceeded")
        result = (
            Instance(path, np.zeros(3), UPRIGHT, 1.0, 0.0, 0.0, np.zeros(3))
            if len(path) == 1
            else self._place(path)
        )
        self._poses[path] = result
        key = path[-1][0]
        self._pose_counts[key] = self._pose_counts.get(key, 0) + 1
        return result

    def _place(self, path):
        parent = self.instance(path[:-1])
        node, slot = self.by_id[path[-1][0]], path[-1][1]
        placement, parent_node = node.placement, self.by_id[path[-2][0]]
        parameter = placement.parameter(slot)
        if parent.length:
            twist = parent_node.shape.twist if parent_node.kind == "blade" else 0
            centre, tangent, lateral, normal = arc_frame(
                parent.length, parent.bend, twist, parameter
            )
            if parent_node.kind == "frond":
                angle = parent_node.shape.guide.roll.evaluate(parameter)
                lateral, normal = (
                    np.cos(angle) * lateral + np.sin(angle) * normal,
                    -np.sin(angle) * lateral + np.cos(angle) * normal,
                )
        else:
            centre, tangent, lateral, normal = (
                np.zeros(3),
                np.array((0, 1, 0)),
                np.array((1, 0, 0)),
                np.array((0, 0, 1)),
            )
        entropy = [self.seed, *(value for pair in path for value in pair)]
        rng = np.random.default_rng(np.random.SeedSequence(entropy))
        phase_rng = np.random.default_rng(np.random.SeedSequence(entropy[:-1]))
        azimuth = (
            phase_rng.uniform(-np.pi, np.pi)
            + slot * placement.azimuth_step
            + rng.uniform(-placement.azimuth_variance, placement.azimuth_variance)
        )
        pitch = placement.pitch + rng.uniform(
            -placement.pitch_variance, placement.pitch_variance
        )
        radial = np.cos(azimuth) * lateral + np.sin(azimuth) * normal
        direction = np.cos(pitch) * tangent + np.sin(pitch) * radial
        normal = np.cos(pitch) * radial - np.sin(pitch) * tangent
        frame = parent.frame @ np.stack(
            (np.cross(tangent, radial), direction, normal), axis=-1
        )
        scale = float(placement.scale.evaluate(parameter)) * (
            1 + rng.uniform(-placement.scale_variance, placement.scale_variance)
        )
        anchor = parent.origin + parent.frame @ centre
        petiole = node.shape.petiole if node.kind == "blade" else 0
        origin = anchor + petiole * scale * frame[:, 1]
        return Instance(
            path,
            origin,
            frame,
            scale,
            node.shape.length * scale,
            node.shape.bend,
            anchor,
        )

    def instances(self, node_id, max_new_poses):
        if (
            type(node_id) is not int
            or node_id not in self.by_id
            or type(max_new_poses) is not int
            or max_new_poses < 0
        ):
            raise ValueError(
                "known generator and nonnegative integer pose budget required"
            )
        if self.counts[node_id] == 0:
            return ()
        if self.counts[node_id] > MAX_POSES:
            return None
        ancestors, pending = set(), [node_id]
        while pending:
            key = pending.pop()
            if key in ancestors or self.counts[key] == 0:
                continue
            ancestors.add(key)
            pending.extend(self.by_id[key].parents)
        missing = sum(
            self.counts[key] - self._pose_counts.get(key, 0) for key in ancestors
        )
        if missing > max_new_poses or len(self._poses) + missing > MAX_POSES:
            return None
        return tuple(self.instance(path) for path in self.paths(node_id))
