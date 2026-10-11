"""Lazy conserved stand counts in metres, Z up; spatial IDs ignore the camera.

This experiment proves demand and site identity, not a forest's rendered LOD quality.
"""

import hashlib
import json
from dataclasses import dataclass

import numpy as np
from demand import box_distance


@dataclass(frozen=True)
class Cell:
    level: int
    x: int
    y: int
    counts: tuple

    @property
    def key(self):
        return self.level, self.x, self.y


class StandPlan:
    def __init__(self, counts, heights, spreads, size=512.0, levels=4, seed=7):
        if not counts or any(
            not isinstance(n, (int, np.integer)) or not 0 <= n <= np.iinfo(np.int64).max
            for n in counts
        ):
            raise ValueError("nonnegative integer counts required")
        if len(counts) != len(heights) or len(counts) != len(spreads):
            raise ValueError("one height and spread per plant recipe required")
        if not all(np.isfinite(v) and v > 0 for v in (*heights, *spreads, size)):
            raise ValueError("positive finite dimensions required")
        if not isinstance(levels, int) or not 0 <= levels <= 20:
            raise ValueError("bounded canonical site level required")
        self.root = Cell(0, 0, 0, tuple(int(n) for n in counts))
        self.heights = np.array(heights, dtype=np.float64, copy=True)
        self.spreads = np.array(spreads, dtype=np.float64, copy=True)
        self.heights.setflags(write=False)
        self.spreads.setflags(write=False)
        self.size, self.levels, self.seed = size, levels, seed
        inputs = [
            1,
            np.__version__,
            self.root.counts,
            self.heights.tolist(),
            self.spreads.tolist(),
            size,
            levels,
            seed,
        ]
        self.identity = hashlib.sha256(
            json.dumps(inputs, separators=(",", ":")).encode()
        ).hexdigest()

    def random(self, cell, purpose):
        key = f"{self.identity}:{cell.key}:{purpose}".encode()
        seed = int.from_bytes(hashlib.blake2b(key, digest_size=8).digest(), "little")
        return np.random.default_rng(seed)

    def bounds(self, cell):
        span = self.size / (1 << cell.level)
        anchor = np.array([cell.x, cell.y]) * span - self.size / 2
        radius = float(np.max(self.spreads)) / 2
        lower = np.array([anchor[0] - radius, anchor[1] - radius, 0.0])
        upper = np.array(
            [
                anchor[0] + span + radius,
                anchor[1] + span + radius,
                float(np.max(self.heights)),
            ]
        )
        return lower, upper

    def children(self, cell):
        if cell.level >= self.levels:
            return ()
        random = self.random(cell, "counts")
        counts = np.array([random.multinomial(n, [0.25] * 4) for n in cell.counts]).T
        return tuple(
            Cell(
                cell.level + 1,
                cell.x * 2 + index % 2,
                cell.y * 2 + index // 2,
                tuple(int(n) for n in row),
            )
            for index, row in enumerate(counts)
        )

    def sites(self, cell):
        if cell.level != self.levels:
            raise ValueError("sites require the canonical leaf cell")
        span = self.size / (1 << cell.level)
        anchor = np.array([cell.x, cell.y]) * span - self.size / 2
        positions, kinds, ids = [], [], []
        for kind, count in enumerate(cell.counts):
            xy = anchor + self.random(cell, f"sites:{kind}").random((count, 2)) * span
            positions.append(np.column_stack((xy, np.zeros(count))))
            kinds.extend([kind] * count)
            ids.extend((*cell.key, kind, ordinal) for ordinal in range(count))
        return {
            "positions": np.concatenate(positions),
            "kinds": np.asarray(kinds, dtype=np.int16),
            "ids": tuple(ids),
        }

    def needed_members(self, sites, camera, distance):
        radii, heights = self.spreads[sites["kinds"]] / 2, self.heights[sites["kinds"]]
        padding = np.column_stack((radii, radii, np.zeros(len(radii))))
        lower = sites["positions"] - padding
        upper = sites["positions"] + padding
        upper[:, 2] += heights
        return camera.visible(lower, upper) & (
            box_distance(camera.position, lower, upper) <= distance
        )


class Products:
    def __init__(self, plan):
        self.plan, self.records = plan, {}
        self.generated = {"field": 0, "sites": 0, "members": 0}

    def field(self, cell):
        key = "field", cell.key
        if key not in self.records:
            counts = np.asarray(cell.counts)
            self.records[key] = {
                "cell": cell,
                "bounds": self.plan.bounds(cell),
                "children": self.plan.children(cell),
                "crown_area_m2": counts * np.pi * (self.plan.spreads / 2) ** 2,
                "height_sum_m": counts * self.plan.heights,
            }
            self.generated["field"] += 1
        return self.records[key]

    def sites(self, cell):
        key = "sites", cell.key
        if key not in self.records:
            self.records[key] = self.plan.sites(cell)
            self.generated["sites"] += 1
            self.generated["members"] += sum(cell.counts)
        return self.records[key]

    def select(
        self, camera, pixels=64, detail_distance=40, node_budget=4096, site_budget=4096
    ):
        if pixels <= 0 or detail_distance < 0 or node_budget < 1 or site_budget < 1:
            raise ValueError("valid demand and work limits required")
        if not np.isfinite(pixels) or not np.isfinite(detail_distance):
            raise ValueError("finite screen and detail distances required")
        if not all(
            isinstance(v, (int, np.integer)) for v in (node_budget, site_budget)
        ):
            raise ValueError("integer work budgets required")
        pending, selected, needed, visited, deferred = [self.plan.root], [], {}, 0, 0
        remaining_sites = site_budget
        while pending:
            cell = pending.pop()
            visited += 1
            lower, upper = self.plan.bounds(cell)
            if not sum(cell.counts) or not camera.visible(lower, upper):
                continue
            product = self.field(cell)
            near = box_distance(camera.position, lower, upper) <= detail_distance
            refine = near or camera.projected_span(lower, upper) > pixels
            children = product["children"]
            if (
                refine
                and children
                and visited + len(pending) + len(children) <= node_budget
            ):
                pending.extend(reversed(children))
                continue
            selected.append(cell)
            if refine and children:
                deferred += 1
            if near and not children:
                cached = ("sites", cell.key) in self.records
                if not cached and sum(cell.counts) > remaining_sites:
                    deferred += 1
                    continue
                sites = self.sites(cell)
                if not cached:
                    remaining_sites -= sum(cell.counts)
                mask = self.plan.needed_members(sites, camera, detail_distance)
                for index in np.flatnonzero(mask):
                    needed[sites["ids"][index]] = sites["positions"][index].copy()
        return {
            "cells": tuple(selected),
            "sites": needed,
            "visited": visited,
            "deferred": deferred,
        }
