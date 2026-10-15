"""Bounded homogeneous Poisson roots on canonical planar metre cells."""

import hashlib
import json
from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class GroundCoverPlan:
    seed: int
    density_m2: float
    cell_m: float = 1.0

    def __post_init__(self):
        if type(self.seed) is not int or not 0 <= self.seed < 2**64:
            raise ValueError("uint64 seed required")
        if not np.isfinite(self.density_m2) or not 0 <= self.density_m2 <= 2048:
            raise ValueError("finite expected root density within0..2048/m2 required")
        if not np.isfinite(self.cell_m) or not 0.25 <= self.cell_m <= 16:
            raise ValueError("finite canonical cell side within0.25..16m required")

    def recipe(self):
        return {
            "format": "ground-cover-plan-1",
            "numpy": np.__version__,
            "seed": self.seed,
            "density_m2": float(self.density_m2),
            "cell_m": float(self.cell_m),
        }

    @classmethod
    def from_recipe(cls, recipe):
        if (
            recipe["format"] != "ground-cover-plan-1"
            or recipe["numpy"] != np.__version__
        ):
            raise ValueError("matching versioned Poisson/RNG replay required")
        return cls(recipe["seed"], recipe["density_m2"], recipe["cell_m"])

    @property
    def identity(self):
        return hashlib.sha256(
            json.dumps(self.recipe(), sort_keys=True).encode()
        ).hexdigest()

    def _random(self, cell, purpose):
        prefix = b"outshine-cover-1\0" + self.seed.to_bytes(8, "little")
        key = prefix + np.asarray(cell, dtype="<i8").tobytes() + purpose
        seed = int.from_bytes(hashlib.blake2b(key, digest_size=16).digest(), "little")
        return np.random.Generator(np.random.PCG64(seed))

    def _region(self, lower, upper, max_cells):
        lower, upper = np.asarray(lower, float), np.asarray(upper, float)
        if (
            lower.shape != (2,)
            or upper.shape != (2,)
            or not np.isfinite([lower, upper]).all()
            or np.any(abs(lower) > 10_000_000)
            or np.any(abs(upper) > 10_000_000)
            or np.any(upper < lower)
            or type(max_cells) is not int
            or not 0 <= max_cells <= 65536
        ):
            raise ValueError(
                "ordered finite planar metre bounds and bounded cell budget required"
            )
        first = np.floor(lower / self.cell_m).astype(np.int64)
        last = np.ceil(upper / self.cell_m).astype(np.int64)
        if np.any(lower == upper):
            return lower, upper, np.empty((0, 2), np.int64)
        if int(np.prod(last - first)) > max_cells:
            return None
        x, y = np.meshgrid(
            np.arange(first[0], last[0]), np.arange(first[1], last[1]), indexing="ij"
        )
        return lower, upper, np.column_stack((x.ravel(), y.ravel()))

    def summary(self, lower, upper, max_cells=4096):
        region = self._region(lower, upper, max_cells)
        if region is None:
            return None
        lower, upper, cells = region
        counts = np.fromiter(
            (
                self._random(cell, b"count").poisson(self.density_m2 * self.cell_m**2)
                for cell in cells
            ),
            dtype=np.int64,
            count=len(cells),
        )
        return {
            "cells": cells,
            "counts": counts,
            "candidate_roots": int(counts.sum()),
            "expected_roots_in_query": float(np.prod(upper - lower) * self.density_m2),
        }

    def sites(self, lower, upper, max_sites=1_000_000, max_cells=4096):
        if type(max_sites) is not int or not 0 <= max_sites <= 1_000_000:
            raise ValueError("candidate-root budget within0..1000000 required")
        summary = self.summary(lower, upper, max_cells)
        if summary is None or summary["candidate_roots"] > max_sites:
            return None
        total = summary["candidate_roots"]
        xy = np.empty((total, 2))
        ids = np.empty((total, 3), np.int64)
        marks = np.empty((total, 3))
        begin = 0
        for cell, count in zip(summary["cells"], summary["counts"], strict=True):
            end = begin + int(count)
            u = self._random(cell, b"sites").random((count, 5))
            xy[begin:end] = cell * self.cell_m + u[:, :2] * self.cell_m
            ids[begin:end, :2] = cell
            ids[begin:end, 2] = np.arange(count)
            marks[begin:end] = u[:, 2:]
            begin = end
        selected = np.all((xy >= lower) & (xy < upper), axis=1)
        return {"ids": ids[selected], "xy": xy[selected], "marks": marks[selected]}
