"""Bounded host→short shoot→leaf hierarchy over unchanged physical leaf IDs."""

import hashlib
import json
from dataclasses import dataclass, field
from functools import cached_property
from types import MappingProxyType

import numpy as np

from .blade_surface import Blade
from .curve_frame import arc_frame
from .plan_arrays import immutable_array
from .shoot_plan import Shoot, normal_frame


@dataclass(frozen=True)
class TwigClusters:
    host_ids: np.ndarray
    host_counts: np.ndarray
    variants: np.ndarray
    origins: np.ndarray
    frames: np.ndarray
    source_identity: str
    seed: int = 718
    leaf_limit: int = 6
    carrier_length: float = 0.16
    carrier_bend: float = 0.4
    carrier_radii: tuple = (0.0005, 0.00012)
    internode: float = 0.04
    petiole: float = 0.004
    _prototypes: dict = field(
        default_factory=dict, init=False, repr=False, compare=False
    )
    _groups: dict = field(default_factory=dict, init=False, repr=False, compare=False)

    def __post_init__(self):
        ids, counts, variants = map(
            np.asarray, (self.host_ids, self.host_counts, self.variants)
        )
        origins, frames = (np.asarray(a, float) for a in (self.origins, self.frames))
        radii = np.asarray(self.carrier_radii, float)
        if (
            ids.ndim != 1
            or not 1 <= len(ids) <= 65536
            or ids.dtype.kind not in "iu"
            or np.any(ids < 0)
            or np.any(ids >= 2**58)
            or np.any(ids[1:] <= ids[:-1])
            or counts.shape != ids.shape
            or counts.dtype.kind not in "iu"
            or np.any(counts < 0)
            or np.any(counts > 128)
            or variants.ndim != 1
            or variants.dtype.kind not in "iu"
            or len(variants) > 100000
            or np.any(variants < 0)
            or np.any(variants >= 64)
            or counts.sum() != len(variants)
            or origins.shape != (len(ids), 3)
            or frames.shape != (len(ids), 3, 3)
            or not np.isfinite(origins).all()
            or not np.isfinite(frames).all()
            or np.max(abs(origins)) > 100000
            or not np.allclose(
                frames @ frames.transpose(0, 2, 1), np.eye(3), rtol=0, atol=1e-12
            )
            or not np.allclose(np.linalg.det(frames), 1, rtol=0, atol=1e-12)
            or radii.shape != (2,)
            or not np.isfinite(radii).all()
            or not 0 < radii[1] <= radii[0]
            or abs(self.carrier_bend) * radii[0] >= self.carrier_length
            or not isinstance(self.source_identity, str)
            or len(self.source_identity) != 64
            or type(self.seed) is not int
            or not 0 <= self.seed < 2**64
            or type(self.leaf_limit) is not int
            or not 4 <= self.leaf_limit <= 16
            or not np.isfinite(
                (self.carrier_length, self.carrier_bend, self.internode, self.petiole)
            ).all()
            or not 0.01 <= self.carrier_length <= 2
            or abs(self.carrier_bend) > 1
            or not 0.01 <= self.internode <= 0.1
            or not 0 <= self.petiole <= 0.1
        ):
            raise ValueError(
                "bounded canonical host/leaf counts, rigid frames and physical recipe required"
            )
        twigs = (counts + self.leaf_limit - 1) // self.leaf_limit
        if twigs.sum() > 16384 or np.any(twigs > 32):
            raise ValueError("complete short-shoot hierarchy exceeds declared capacity")
        nonempty = twigs > 0
        if np.any(nonempty):
            minimum = int(np.min(counts[nonempty] // twigs[nonempty]))
            minimum_length = (minimum - 0.5) * self.internode / 0.9 * 0.85
            if 0.65 * radii[0] > minimum_length / 8:
                raise ValueError(
                    "carrier radius exceeds the slender short-shoot capacity"
                )
        object.__setattr__(self, "carrier_radii", tuple(map(float, radii)))
        for key, value in (
            ("host_ids", ids.astype(np.uint64)),
            ("host_counts", counts.astype(np.int32)),
            ("variants", variants.astype(np.uint8)),
            ("origins", origins),
            ("frames", frames),
        ):
            object.__setattr__(self, key, immutable_array(value))

    @cached_property
    def offsets(self):
        return immutable_array(np.r_[0, np.cumsum(self.host_counts)])

    def recipe(self):
        return {
            "format": "twig-clusters-1",
            "numpy": np.__version__,
            "source_identity": self.source_identity,
            "seed": self.seed,
            "leaf_limit": self.leaf_limit,
            "carrier_length": self.carrier_length,
            "carrier_bend": self.carrier_bend,
            "carrier_radii": list(self.carrier_radii),
            "internode": self.internode,
            "petiole": self.petiole,
        }

    @cached_property
    def identity(self):
        digest = hashlib.sha256(json.dumps(self.recipe(), sort_keys=True).encode())
        for a in (
            self.host_ids,
            self.host_counts,
            self.variants,
            self.origins,
            self.frames,
        ):
            digest.update(a.view(np.uint8))
        return digest.hexdigest()

    def summary(self):
        return {
            "identity": self.identity,
            "hosts": len(self.host_ids),
            "twigs": int(
                ((self.host_counts + self.leaf_limit - 1) // self.leaf_limit).sum()
            ),
            "leaves": len(self.variants),
            "prototype_capacity": 16 * self.leaf_limit,
        }

    def _index(self, node):
        if type(node) is not int:
            raise ValueError("explicit canonical host ID required")
        index = int(np.searchsorted(self.host_ids, node))
        if index >= len(self.host_ids) or self.host_ids[index] != node:
            raise ValueError("unknown canonical host ID")
        return index

    def twigs(self, node, max_twigs):
        index = self._index(node)
        count = int(self.host_counts[index])
        n = (count + self.leaf_limit - 1) // self.leaf_limit
        if type(max_twigs) is not int or not 0 <= max_twigs <= 16384:
            raise ValueError("explicit bounded twig capacity required")
        if n > max_twigs:
            return None
        rng = np.random.default_rng(
            np.random.SeedSequence((self.seed, node, 0x54574947))
        )
        slots = (np.arange(n) + 0.5) / max(1, n)
        t = 0.12 + 0.76 * (slots + rng.uniform(-0.25, 0.25, n) / max(1, n))
        centre, tangent, lateral, normal = arc_frame(
            self.carrier_length, self.carrier_bend, 0, t
        )
        phi = (
            rng.uniform(0, 2 * np.pi)
            + np.arange(n) * np.pi * (3 - np.sqrt(5))
            + rng.uniform(-0.25, 0.25, n)
        )
        beta = rng.uniform(0.65, 1.25, n)
        direction = tangent * np.cos(beta)[:, None] + np.sin(beta)[:, None] * (
            lateral * np.cos(phi)[:, None] + normal * np.sin(phi)[:, None]
        )
        local = normal_frame(direction, normal)
        counts = np.full(n, count // max(1, n), np.int32)
        counts[: count % max(1, n)] += 1
        variants = rng.integers(0, 16, n, dtype=np.uint8)
        return {
            "ids": np.uint64(node) * np.uint64(32) + np.arange(n, dtype=np.uint64),
            "counts": counts,
            "variants": variants,
            "parameters": t,
            "origins": centre @ self.frames[index].T + self.origins[index],
            "frames": self.frames[index] @ local,
            "radii": 0.65
            * (
                self.carrier_radii[0]
                + (self.carrier_radii[1] - self.carrier_radii[0]) * t
            ),
        }

    def shoot(self, count, variant):
        if (
            type(count) is not int
            or not 1 <= count <= self.leaf_limit
            or type(variant) is not int
            or not 0 <= variant < 16
        ):
            raise ValueError("bounded physical prototype count and variation required")
        length = (count - 0.5) * self.internode / 0.9 * (0.85 + 0.3 * variant / 15)
        return Shoot(
            seed=(self.seed ^ ((variant + 1) * 0x9E3779B97F4A7C15)) % 2**64,
            length=length,
            internode=length * 0.9 / (count - 0.5),
            first=0.05,
            last=0.95,
            bend=0.35,
            normal_hint=(0, 0, 1),
            normal_alignment=0.3,
            blade=Blade(petiole=self.petiole),
        )

    def prototype(self, count, variant, reuse=True):
        key = count, variant
        if not reuse:
            return self.shoot(count, variant).detail(count)
        if key not in self._prototypes:
            self._prototypes[key] = self.shoot(count, variant).detail(count)
        return self._prototypes[key]

    def group(self, node, max_leaves, *, reuse=True):
        index = self._index(node)
        count = int(self.host_counts[index])
        if type(max_leaves) is not int or not 0 <= max_leaves <= 100000:
            raise ValueError("explicit bounded leaf capacity required")
        if count > max_leaves:
            return None
        if reuse and node in self._groups:
            return self._groups[node]
        twigs = self.twigs(node, 32)
        origins, frames, nodes, owners = [], [], [], []
        for i, n in enumerate(twigs["counts"]):
            local = self.prototype(int(n), int(twigs["variants"][i]), reuse)
            origin, frame = twigs["origins"][i], twigs["frames"][i]
            origins.append(local["origins"] @ frame.T + origin)
            nodes.append(local["nodes"] @ frame.T + origin)
            frames.append(frame @ local["frames"])
            owners.append(np.full(n, twigs["ids"][i], np.uint64))
        first, last = map(int, self.offsets[index : index + 2])
        result = {
            "ids": np.arange(first, last, dtype=np.uint64),
            "variants": self.variants[first:last],
            "origins": np.concatenate(origins) if count else np.empty((0, 3)),
            "frames": np.concatenate(frames) if count else np.empty((0, 3, 3)),
            "nodes": np.concatenate(nodes) if count else np.empty((0, 3)),
            "twig_ids": np.concatenate(owners) if count else np.empty(0, np.uint64),
        }
        result = MappingProxyType({k: immutable_array(v) for k, v in result.items()})
        if reuse:
            self._groups[node] = result
            return self._groups[node]
        return result

    def tables(self, max_leaves, *, reuse=True):
        if type(max_leaves) is not int or not 0 <= max_leaves <= 100000:
            raise ValueError("explicit bounded whole-leaf capacity required")
        if len(self.variants) > max_leaves:
            return None
        groups = [
            self.group(int(node), max_leaves, reuse=reuse) for node in self.host_ids
        ]
        return {k: np.concatenate([group[k] for group in groups]) for k in groups[0]}
