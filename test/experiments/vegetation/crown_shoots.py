"""Stable demanded end-shoot poses on a saved spatial skeleton, not new growth."""

import hashlib
import json
from dataclasses import dataclass, field
from functools import cached_property
from types import MappingProxyType

import numpy as np
from form_plan import validate_parents
from outshine_ref.vegetation import Blade, Shoot
from outshine_ref.vegetation.plan_arrays import immutable_array
from outshine_ref.vegetation.shoot_plan import normal_frame


@dataclass(frozen=True)
class CrownShoots:
    positions: np.ndarray
    parents: np.ndarray
    variants: np.ndarray
    seed: int = 718
    length_m: float = 0.35
    bend_rad: float = 0.4
    petiole_m: float = 0.004
    foliage_depth_m: float = 0.0
    _groups: dict = field(default_factory=dict, init=False, repr=False, compare=False)

    def __post_init__(self):
        positions = np.asarray(self.positions, np.float64)
        parents = np.asarray(self.parents)
        variants = np.asarray(self.variants)
        if parents.ndim != 1 or not 2 <= len(parents) <= 65536:
            raise ValueError("bounded saved spatial skeleton required")
        validate_parents(parents)
        if (
            positions.shape != (len(parents), 3)
            or not np.isfinite(positions).all()
            or np.max(abs(positions)) > 100000
        ):
            raise ValueError(
                "one finite metre-space position per skeleton node required"
            )
        delta = positions[1:] - positions[parents[1:]]
        if np.any(np.linalg.norm(delta, axis=1) <= 1e-8):
            raise ValueError("noncollapsed saved branch edges required")
        if (
            variants.ndim != 1
            or variants.dtype.kind not in "iu"
            or len(variants) > 100000
            or np.any(variants < 0)
            or np.any(variants >= 64)
        ):
            raise ValueError("bounded explicit source leaf variants required")
        if (
            type(self.seed) is not int
            or not 0 <= self.seed < 2**64
            or not np.isfinite(
                (self.length_m, self.bend_rad, self.petiole_m, self.foliage_depth_m)
            ).all()
            or not 0.01 <= self.length_m <= 2
            or abs(self.bend_rad) > 1
            or not 0 <= self.petiole_m <= 0.1
            or not 0 <= self.foliage_depth_m <= 5
        ):
            raise ValueError("bounded explicit end-shoot recipe required")
        object.__setattr__(self, "positions", immutable_array(positions))
        object.__setattr__(self, "parents", immutable_array(parents.astype(np.int32)))
        object.__setattr__(self, "variants", immutable_array(variants.astype(np.uint8)))

    @cached_property
    def tips(self):
        counts = np.bincount(self.parents[1:], minlength=len(self.parents))
        return immutable_array(np.flatnonzero(counts == 0))

    @cached_property
    def hosts(self):
        distance = np.full(len(self.parents), np.inf)
        distance[self.tips] = 0
        for node in range(len(self.parents) - 1, 0, -1):
            parent = self.parents[node]
            length = np.linalg.norm(self.positions[node] - self.positions[parent])
            distance[parent] = min(distance[parent], distance[node] + length)
        return immutable_array(
            np.flatnonzero(
                (distance <= self.foliage_depth_m) & (np.arange(len(distance)) > 0)
            )
        )

    @cached_property
    def counts(self):
        counts = np.full(
            len(self.hosts), len(self.variants) // len(self.hosts), np.int64
        )
        counts[: len(self.variants) % len(counts)] += 1
        return immutable_array(counts)

    @cached_property
    def offsets(self):
        return immutable_array(np.r_[0, np.cumsum(self.counts)])

    def recipe(self):
        return {
            "format": "spatial-crown-shoots-2",
            "numpy": np.__version__,
            "seed": self.seed,
            "length_m": self.length_m,
            "bend_rad": self.bend_rad,
            "petiole_m": self.petiole_m,
            "foliage_depth_m": self.foliage_depth_m,
        }

    @cached_property
    def identity(self):
        digest = hashlib.sha256(json.dumps(self.recipe(), sort_keys=True).encode())
        for a in (self.positions, self.parents, self.variants):
            digest.update(a.tobytes())
        return digest.hexdigest()

    def summary(self):
        return {
            "identity": self.identity,
            "skeleton_nodes": len(self.parents),
            "shoots": len(self.hosts),
            "terminal_nodes": len(self.tips),
            "leaves": len(self.variants),
            "variant_counts": np.bincount(self.variants, minlength=8).tolist(),
        }

    def stem(self, node):
        if type(node) is not int or node not in self.hosts:
            raise ValueError("explicit saved foliage host node ID required")
        direction = self.positions[node] - self.positions[self.parents[node]]
        return self.positions[node], normal_frame(direction, (0, 0, 1))

    def group(self, node, max_leaves):
        if type(max_leaves) is not int or not 0 <= max_leaves <= 100000:
            raise ValueError("bounded explicit leaf capacity required")
        origin, frame = self.stem(node)
        index = int(np.searchsorted(self.hosts, node))
        count = int(self.counts[index])
        if count > max_leaves:
            return None
        if node not in self._groups:
            begin, end = map(int, self.offsets[index : index + 2])
            ids = np.arange(begin, end, dtype=np.uint64)
            if count:
                local = Shoot(
                    seed=(self.seed ^ node),
                    length=self.length_m,
                    internode=self.length_m * 0.9 / max(0.5, count - 0.5),
                    first=0.05,
                    last=0.95,
                    bend=self.bend_rad,
                    normal_hint=(0, 0, 1),
                    normal_alignment=0.3,
                    blade=Blade(petiole=self.petiole_m),
                ).detail(count)
                if local is None or len(local["ids"]) != count:
                    raise ValueError("end-shoot count recipe changed")
                positions = local["origins"] @ frame.T + origin
                nodes = local["nodes"] @ frame.T + origin
                frames = np.einsum("ij,njk->nik", frame, local["frames"])
            else:
                positions, nodes, frames = (
                    np.empty((0, 3)),
                    np.empty((0, 3)),
                    np.empty((0, 3, 3)),
                )
            self._groups[node] = MappingProxyType(
                {
                    k: immutable_array(v)
                    for k, v in {
                        "ids": ids,
                        "variants": self.variants[begin:end],
                        "origins": positions,
                        "nodes": nodes,
                        "frames": frames,
                        "owner_nodes": np.full(count, node, np.int32),
                    }.items()
                }
            )
        return self._groups[node]

    def tables(self, max_leaves):
        if type(max_leaves) is not int or not 0 <= max_leaves <= 100000:
            raise ValueError("bounded explicit whole leaf capacity required")
        if len(self.variants) > max_leaves:
            return None
        groups = [self.group(int(node), max_leaves) for node in self.hosts]
        return {k: np.concatenate([group[k] for group in groups]) for k in groups[0]}
