"""Canonical generator topology, bounded instance requests and stable hierarchy paths."""

import hashlib
import heapq
import json
from dataclasses import dataclass, field
from functools import cached_property
from types import MappingProxyType

import numpy as np
from bend_pattern import BendPattern
from generator_specs import FrondShape, Generator, TubeShape
from graph_frames import parent_frame, parent_frames
from needle_surface import Needle
from spatial_axis import SpatialAxis

UPRIGHT = np.array(((1, 0, 0), (0, 0, -1), (0, 1, 0)), dtype=float)
MAX_POSES = 100_000


def shape_bound(shape):
    if shape is None:
        return 0.0
    if isinstance(shape, Needle):
        return shape.length + shape.radius
    if isinstance(shape, SpatialAxis):
        return np.linalg.norm(
            np.asarray(shape.spine.controls), axis=-1
        ).max() + shape.radius.table.max() * max(shape.ellipse)
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
    spatial_batches: bool = field(default=True, compare=False)
    _poses: dict = field(default_factory=dict, init=False, repr=False, compare=False)
    _pose_counts: dict = field(
        default_factory=dict, init=False, repr=False, compare=False
    )
    _sites: dict = field(default_factory=dict, init=False, repr=False, compare=False)
    _complete_nodes: set = field(
        default_factory=set, init=False, repr=False, compare=False
    )
    _site_rows: int = field(default=0, init=False, repr=False, compare=False)
    _parent_frames: dict = field(
        default_factory=dict, init=False, repr=False, compare=False
    )

    def __post_init__(self):
        if (
            type(self.seed) is not int
            or not 0 <= self.seed < 2**64
            or not isinstance(self.nodes, tuple)
            or not 1 <= len(self.nodes) <= 64
            or type(self.spatial_batches) is not bool
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
        if sum(self.capacities.values()) >= 2**64:
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
    def capacities(self):
        result = {}
        for key in self.order:
            node = self.by_id[key]
            result[key] = (
                sum(
                    node.placement.capacity(self.length_bounds[p]) * result[p]
                    for p in node.parents
                )
                if node.parents
                else 1
            )
        return MappingProxyType(result)

    @cached_property
    def slot_capacities(self):
        return MappingProxyType(
            {
                n.id: max(
                    (n.placement.capacity(self.length_bounds[p]) for p in n.parents),
                    default=1,
                )
                for n in self.nodes
            }
        )

    @cached_property
    def length_bounds(self):
        return MappingProxyType(
            {
                n.id: n.shape.length
                * n.placement.scale.table.max()
                * (1 + n.placement.scale_variance)
                if n.shape is not None
                else 0.0
                for n in self.nodes
            }
        )

    @cached_property
    def bounds(self):
        result = {}
        for key in self.order:
            node = self.by_id[key]
            if self.capacities[key] == 0:
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
        complete = all(
            key in self._complete_nodes or count == 0
            for key, count in self.capacities.items()
        )
        return {
            "identity": self.identity,
            "instances": len(self._poses) if complete else None,
            "counts": {key: self._pose_counts.get(key, 0) for key in self.order}
            if complete
            else None,
            "capacity_counts": dict(self.capacities),
            "bound_radius_m": max(self.bounds.values()),
        }

    def paths(self, node_id):
        if type(node_id) is not int or node_id not in self.by_id:
            raise ValueError("known generator ID required")
        node = self.by_id[node_id]
        if self.capacities[node_id] == 0:
            return
        if node.kind == "root":
            yield ((node_id, 0),)
        else:
            for parent in sorted(node.parents):
                for path in self.paths(parent):
                    count = node.placement.count
                    spatial = (
                        isinstance(self.by_id[parent].shape, SpatialAxis)
                        and self.spatial_batches
                    )
                    if node.placement.pattern is not None or spatial:
                        pose = self.instance(path)
                        key = (path, node_id)
                        table = self._sites.get(key)
                        if table is None:
                            table = self.site_table(node_id, pose)
                            self.remember_sites(key, table)
                        count = len(table)
                        if (
                            spatial
                            and key in self._sites
                            and key not in self._parent_frames
                        ):
                            self._parent_frames[key] = parent_frames(
                                self.by_id[parent], pose, table[:, 0]
                            )
                    for slot in range(count):
                        yield (*path, (node_id, slot))

    def site_table(self, node_id, parent):
        seed = np.random.SeedSequence(
            [self.seed, node_id, *(v for pair in parent.path for v in pair), 0x53495445]
        )
        return self.by_id[node_id].placement.sites(
            parent.length, seed, self.by_id[parent.path[-1][0]].shape
        )

    def remember_sites(self, key, table):
        if key not in self._sites and self._site_rows + len(table) <= MAX_POSES:
            self._sites[key] = table
            object.__setattr__(self, "_site_rows", self._site_rows + len(table))

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
                or not 0 <= slot < self.slot_capacities[key]
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
        staged, sites = {}, {}
        for end in range(1, len(path) + 1):
            prefix = path[:end]
            if prefix in self._poses:
                continue
            if end == 1:
                result = Instance(
                    prefix, np.zeros(3), UPRIGHT, 1.0, 0.0, 0.0, np.zeros(3)
                )
            else:
                parent = staged.get(prefix[:-1], self._poses.get(prefix[:-1]))
                key = (prefix[:-1], prefix[-1][0])
                table = self._sites.get(key)
                if table is None:
                    table = sites[key] = self.site_table(prefix[-1][0], parent)
                if prefix[-1][1] >= len(table):
                    raise ValueError("site does not exist on this particular parent")
                result = self._place(prefix, parent, table[prefix[-1][1]])
            staged[prefix] = result
        for key, table in sites.items():
            self.remember_sites(key, table)
        for prefix, result in staged.items():
            self._poses[prefix] = result
            key = prefix[-1][0]
            self._pose_counts[key] = self._pose_counts.get(key, 0) + 1
        return self._poses[path]

    def _place(self, path, parent, site):
        node = self.by_id[path[-1][0]]
        placement, parent_node = node.placement, self.by_id[path[-2][0]]
        parameter, site_angle = site
        prepared = self._parent_frames.get((parent.path, node.id))
        centre, tangent, lateral, normal = (
            parent_frame(parent_node, parent, parameter)
            if prepared is None
            else prepared[path[-1][1]]
        )
        entropy = [self.seed, *(value for pair in path for value in pair)]
        rng = np.random.default_rng(np.random.SeedSequence(entropy))
        phase_rng = np.random.default_rng(np.random.SeedSequence(entropy[:-1]))
        azimuth = (
            (
                0.0
                if isinstance(placement.pattern, BendPattern)
                else phase_rng.uniform(-np.pi, np.pi)
            )
            + site_angle
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
        if self.capacities[node_id] == 0:
            return ()
        if self.capacities[node_id] > MAX_POSES:
            return None
        ancestors, pending = set(), [node_id]
        while pending:
            key = pending.pop()
            if key in ancestors or self.capacities[key] == 0:
                continue
            ancestors.add(key)
            pending.extend(self.by_id[key].parents)
        missing = sum(
            self.capacities[key] - self._pose_counts.get(key, 0)
            for key in ancestors
            if key not in self._complete_nodes
        )
        if missing > max_new_poses or len(self._poses) + missing > MAX_POSES:
            return None
        result = tuple(self.instance(path) for path in self.paths(node_id))
        self._complete_nodes.update(ancestors)
        return result
