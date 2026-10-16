"""Area-conditioned conserved stand hierarchy; fine roots are explicit demand."""

import hashlib
import json
from dataclasses import dataclass, field
from functools import cached_property
from itertools import pairwise
from types import MappingProxyType

import numpy as np
import shapely
from shapely.geometry.base import BaseGeometry

from .plan_arrays import immutable_array
from .stand_habitat import StandHabitat, polygon_components


@dataclass(frozen=True)
class StandCell:
    key: tuple[int, int, int]
    counts: tuple[int, ...]
    domain: BaseGeometry
    area_m2: float


@dataclass(frozen=True)
class HabitatStand:
    """Single-owner lazy plan. Kinds/counts are explicit, not inferred species observations.

    Size descriptors are metres above caller-owned terrain; returned sites are XY.
    Field cover approximates overlapping random crowns, not certified coverage/radiance.
    Geometry capacities bound WKB payload, not GEOS/Python/allocator peak memory.
    """

    habitat: StandHabitat
    kinds: tuple[str, ...]
    counts: tuple[int, ...]
    heights_m: tuple[float, ...]
    spreads_m: tuple[float, ...]
    levels: int = 6
    seed: int = 7
    max_cells: int = 4096
    max_cached_sites: int = 1_000_000
    max_geometry_bytes: int = 16 * 1024**2
    _cells: dict = field(default_factory=dict, init=False, repr=False, compare=False)
    _fields: dict = field(default_factory=dict, init=False, repr=False, compare=False)
    _sites: dict = field(default_factory=dict, init=False, repr=False, compare=False)
    _geometry_bytes: int = field(default=0, init=False, repr=False, compare=False)
    _site_count: int = field(default=0, init=False, repr=False, compare=False)

    def __post_init__(self):
        if not isinstance(self.habitat, StandHabitat):
            raise TypeError("prepared source habitat required")
        if (
            not isinstance(self.kinds, tuple)
            or any(
                not isinstance(v, str) or not 1 <= len(v.encode()) <= 512
                for v in self.kinds
            )
            or tuple(sorted(set(self.kinds))) != self.kinds
        ):
            raise ValueError(
                "canonical sorted unique bounded kind identifiers required"
            )
        if (
            not isinstance(self.counts, tuple)
            or not 1 <= len(self.counts) <= 64
            or len(self.kinds) != len(self.counts)
            or any(type(n) is not int or not 0 <= n <= 2**40 for n in self.counts)
            or sum(self.counts) > 2**40
        ):
            raise ValueError("bounded explicit integer root counts required")
        for values in (self.heights_m, self.spreads_m):
            if (
                not isinstance(values, tuple)
                or len(values) != len(self.counts)
                or not np.isfinite(values).all()
                or any(not 0 < v <= 150 for v in values)
            ):
                raise ValueError(
                    "one finite positive bounded size per declared kind required"
                )
        object.__setattr__(self, "heights_m", tuple(float(v) for v in self.heights_m))
        object.__setattr__(self, "spreads_m", tuple(float(v) for v in self.spreads_m))
        if (
            type(self.levels) is not int
            or not 0 <= self.levels <= 16
            or type(self.seed) is not int
            or not 0 <= self.seed < 2**64
        ):
            raise ValueError("bounded canonical depth and unsigned seed required")
        if (
            type(self.max_cells) is not int
            or not 1 <= self.max_cells <= 65536
            or type(self.max_cached_sites) is not int
            or not 0 <= self.max_cached_sites <= 1_000_000
        ):
            raise ValueError("explicit bounded retained cell/site capacities required")
        if self.habitat.area_m2 == 0 and any(self.counts):
            raise ValueError("empty habitat cannot support nonzero root counts")
        if (
            type(self.max_geometry_bytes) is not int
            or not 0 <= self.max_geometry_bytes <= 256 * 1024**2
        ):
            raise ValueError("bounded retained geometry payload capacity required")
        payload = len(self.habitat.source_geometry.wkb) + len(self.habitat.domain.wkb)
        if payload > self.max_geometry_bytes:
            raise ValueError("prepared root exceeds geometry payload capacity")
        object.__setattr__(self, "_geometry_bytes", payload)
        self._cells[(0, 0, 0)] = StandCell(
            (0, 0, 0), self.counts, self.habitat.domain, self.habitat.area_m2
        )

    @cached_property
    def layout_identity(self):
        value = {
            "format": "habitat-stand-layout-1",
            "numpy": np.__version__,
            "geos": shapely.geos_version_string,
            "habitat": self.habitat.layout,
            "counts": self.counts,
            "kinds": self.kinds,
            "levels": self.levels,
            "seed": self.seed,
        }
        return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()

    @cached_property
    def identity(self):
        value = (self.layout_identity, self.heights_m, self.spreads_m)
        return hashlib.sha256(json.dumps(value).encode()).hexdigest()

    def _random(self, key, purpose):
        value = (self.layout_identity + ":" + str(key) + ":" + purpose).encode()
        return np.random.default_rng(
            int.from_bytes(hashlib.blake2b(value, digest_size=8).digest(), "little")
        )

    def _key(self, key):
        if (
            not isinstance(key, tuple)
            or len(key) != 3
            or any(type(n) is not int for n in key)
        ):
            raise ValueError("integer (level,x,y) canonical key required")
        level, x, y = key
        if (
            not 0 <= level <= self.levels
            or not 0 <= x < 1 << level
            or not 0 <= y < 1 << level
        ):
            raise ValueError("cell key outside canonical hierarchy")

    def _bounds(self, key):
        level, x, y = key
        side = self.habitat.side_m / (1 << level)
        lower = np.asarray(self.habitat.lower) + side * np.asarray((x, y))
        return lower, lower + side

    def _split(self, parent):
        level, x, y = parent.key
        keys = [(level + 1, 2 * x + i % 2, 2 * y + i // 2) for i in range(4)]
        domains = []
        for key in keys:
            lower, upper = self._bounds(key)
            clipped = (
                parent.domain.intersection(shapely.box(*lower, *upper))
                if parent.area_m2
                else shapely.Polygon()
            )
            parts = polygon_components(clipped)
            domains.append(shapely.union_all(parts) if parts else shapely.Polygon())
        areas = np.array([p.area for p in domains])
        if abs(float(areas.sum()) - parent.area_m2) > max(1e-8, parent.area_m2 * 1e-10):
            raise ValueError("child habitats do not partition parent area")
        payload = sum(len(p.wkb) for p in domains)
        if self._geometry_bytes + payload > self.max_geometry_bytes:
            return False
        if parent.area_m2 and any(parent.counts):
            probabilities = areas / areas.sum()
            random = self._random(parent.key, "counts")
            counts = np.array(
                [random.multinomial(n, probabilities) for n in parent.counts], np.uint64
            ).T
        else:
            counts = np.zeros((4, len(self.counts)), np.uint64)
        self._cells.update(
            {
                key: StandCell(key, tuple(int(v) for v in row), domain, float(area))
                for key, row, domain, area in zip(
                    keys, counts, domains, areas, strict=True
                )
            }
        )
        object.__setattr__(self, "_geometry_bytes", self._geometry_bytes + payload)
        return True

    def cell(self, key=(0, 0, 0)):
        """Resolve ancestors/siblings only; return None on cell/geometry capacity."""
        self._key(key)
        if key in self._cells:
            return self._cells[key]
        path = [
            (l, key[1] >> (key[0] - l), key[2] >> (key[0] - l))
            for l in range(key[0] + 1)
        ]
        missing = sum(k not in self._cells for k in path)
        if len(self._cells) + 4 * missing > self.max_cells:
            return None
        for parent, child in pairwise(path):
            if child not in self._cells and not self._split(self._cells[parent]):
                return None
        return self._cells[key]

    def children(self, key=(0, 0, 0)):
        self._key(key)
        if key[0] == self.levels:
            return ()
        level, x, y = key
        keys = [(level + 1, 2 * x + i % 2, 2 * y + i // 2) for i in range(4)]
        # Resolving one path creates all four siblings, with capacity preflight.
        if self.cell(keys[0]) is None:
            return None
        return tuple(self._cells[k] for k in keys)

    def field(self, key=(0, 0, 0)):
        """Cached count/support descriptors; no children below key and no sites."""
        cell = self.cell(key)
        if cell is None:
            return None
        if key not in self._fields:
            counts = np.asarray(cell.counts, dtype=np.uint64)
            total = sum(cell.counts)
            mean_height = (
                float(counts @ np.asarray(self.heights_m) / total) if total else 0.0
            )
            crown_area = float(counts @ (np.pi * (np.asarray(self.spreads_m) / 2) ** 2))
            cover = (
                float(-np.expm1(-crown_area / cell.area_m2)) if cell.area_m2 else 0.0
            )
            lower, upper = self._bounds(key)
            radius = max(self.spreads_m) / 2
            self._fields[key] = MappingProxyType(
                {
                    "cell": key,
                    "frame": self.habitat.frame,
                    "source_id": self.habitat.source_id,
                    "region_id": self.habitat.region_id,
                    "layout_identity": self.layout_identity,
                    "identity": self.identity,
                    "kinds": self.kinds,
                    "domain": cell.domain,
                    "area_m2": cell.area_m2,
                    "counts": immutable_array(counts),
                    "mean_height_m": mean_height,
                    "max_height_m": max(self.heights_m) if total else 0.0,
                    "cover_estimate": cover,
                    "lower_xy_m": immutable_array(lower - radius),
                    "upper_xy_m": immutable_array(upper + radius),
                }
            )
        return self._fields[key]

    def sites(self, key, max_sites=100000, max_triangles=32768):
        """Demand one canonical leaf's immutable XY roots, or None on capacity."""
        self._key(key)
        if (
            key[0] != self.levels
            or type(max_sites) is not int
            or not 0 <= max_sites <= 1_000_000
            or type(max_triangles) is not int
            or not 0 <= max_triangles <= 32768
        ):
            raise ValueError(
                "canonical leaf key and bounded site/triangle capacities required"
            )
        if max_sites == 0 or max_triangles == 0:
            return None
        cell = self.cell(key)
        if cell is None or sum(cell.counts) > max_sites:
            return None
        if key not in self._sites:
            count = sum(cell.counts)
            if self._site_count + count > self.max_cached_sites:
                return None
            from .stand_sites import habitat_sites

            result = habitat_sites(cell, self._random, max_triangles)
            if result is None:
                return None
            self._sites[key] = result
            object.__setattr__(self, "_site_count", self._site_count + count)
        return self._sites[key]

    def recipe(self):
        """Save prepared hierarchy; finished site arrays remain separate asset products."""
        return {
            "format": "habitat-stand-1",
            "numpy": np.__version__,
            "geos": shapely.geos_version_string,
            "habitat": self.habitat.recipe(),
            "counts": list(self.counts),
            "kinds": list(self.kinds),
            "heights_m": list(self.heights_m),
            "spreads_m": list(self.spreads_m),
            "levels": self.levels,
            "seed": self.seed,
            "layout_identity": self.layout_identity,
            "identity": self.identity,
            "prepared_cells": [
                {
                    "key": list(c.key),
                    "counts": list(c.counts),
                    "domain_wkb": c.domain.wkb_hex,
                    "area_m2": c.area_m2,
                }
                for c in sorted(self._cells.values(), key=lambda c: c.key)
                if c.key != (0, 0, 0)
            ],
        }

    @classmethod
    def from_recipe(cls, recipe, **budgets):
        if (
            recipe["format"] != "habitat-stand-1"
            or recipe["numpy"] != np.__version__
            or recipe["geos"] != shapely.geos_version_string
        ):
            raise ValueError("supported versioned habitat stand recipe required")
        result = cls(
            StandHabitat.from_recipe(recipe["habitat"]),
            tuple(recipe["kinds"]),
            tuple(recipe["counts"]),
            tuple(recipe["heights_m"]),
            tuple(recipe["spreads_m"]),
            recipe["levels"],
            recipe["seed"],
            **budgets,
        )
        if (
            result.identity != recipe["identity"]
            or result.layout_identity != recipe["layout_identity"]
        ):
            raise ValueError("prepared stand identities changed")
        result._restore_cells(recipe["prepared_cells"])
        return result

    def _restore_cells(self, records):
        if not isinstance(records, list) or len(records) + 1 > self.max_cells:
            raise ValueError("prepared cell capacity exceeded")
        payload = sum(len(r["domain_wkb"]) // 2 for r in records)
        if self._geometry_bytes + payload > self.max_geometry_bytes:
            raise ValueError("prepared geometry payload capacity exceeded")
        restored = dict(self._cells)
        for record in records:
            key = tuple(record["key"])
            self._key(key)
            counts = tuple(record["counts"])
            if (
                key in restored
                or len(counts) != len(self.counts)
                or any(type(v) is not int or not 0 <= v <= 2**40 for v in counts)
            ):
                raise ValueError("invalid prepared cell identity/counts")
            domain = shapely.from_wkb(record["domain_wkb"])
            area = float(record["area_m2"])
            if (
                domain.geom_type not in ("Polygon", "MultiPolygon")
                or not domain.is_valid
                or (
                    not domain.is_empty
                    and shapely.get_coordinate_dimension(domain) != 2
                )
                or not np.isfinite(shapely.get_coordinates(domain)).all()
                or not np.isfinite(area)
                or area < 0
                or abs(float(domain.area) - area) > max(1e-8, area * 1e-10)
            ):
                raise ValueError("invalid prepared cell domain/area")
            if area == 0 and any(counts):
                raise ValueError("empty prepared cell has members")
            restored[key] = StandCell(key, counts, domain, area)
        for key, parent in restored.items():
            level, x, y = key
            children = [
                restored.get((level + 1, 2 * x + i % 2, 2 * y + i // 2))
                for i in range(4)
            ]
            if any(c is not None for c in children):
                if any(c is None for c in children) or any(
                    sum(c.counts[k] for c in children) != parent.counts[k]
                    for k in range(len(self.counts))
                ):
                    raise ValueError("prepared children do not conserve parent counts")
                if abs(sum(c.area_m2 for c in children) - parent.area_m2) > max(
                    1e-8, parent.area_m2 * 1e-10
                ):
                    raise ValueError("prepared children do not conserve parent area")
            if level and (level - 1, x // 2, y // 2) not in restored:
                raise ValueError("prepared cell lacks parent")
        self._cells.update(restored)
        object.__setattr__(self, "_geometry_bytes", self._geometry_bytes + payload)
