"""Bounded source-aware scalar reconstruction; unresolved regions retain needs."""

import itertools

import numpy as np
import shapely
from source_field_probes import geometry_probes, sample_packets
from source_support_function import sample_support
from trilinear_support import CORNERS, EMPTY, LEAF, SOLID, UNKNOWN, evaluate

GRID = np.array(list(itertools.product([0.0, 0.5, 1.0], repeat=3)))
CORNER_IDS = np.array([0, 2, 6, 8, 18, 20, 24, 26])


class FieldBuilder:
    def __init__(
        self,
        support,
        *,
        max_nodes=4681,
        minimum_side=1.0,
        probe_offset=0.125,
        observer=None,
    ):
        if (
            not 1 <= max_nodes <= 65535
            or not np.isfinite(minimum_side)
            or minimum_side <= 0
        ):
            raise ValueError(
                "bounded field node budget and positive minimum side required"
            )
        self.support = support
        self.observer = observer
        self.boxes, self.owners, probes = geometry_probes(support, offset=probe_offset)
        self.domain_slots = np.repeat(
            np.arange(len(support.source_ids)), np.diff(support.offsets)
        )
        low, high = self.boxes[:, 0].min(0), self.boxes[:, 1].max(0)
        self.side = float(2 ** np.ceil(np.log2(max(float((high - low).max()), 1e-6))))
        self.low = (low + high) / 2 - self.side / 2
        self.depth = max(0, int(np.ceil(np.log2(self.side / minimum_side))))
        if self.depth > 20:
            raise ValueError("bounded dyadic field depth required")
        self.units = 1 << (self.depth + 1)
        self.unit = self.side / self.units
        inside = ((probes >= self.low) & (probes <= self.low + self.side)).all(1)
        self.probes = probes[inside]
        self.work = {
            "source_box_tests": 0,
            "sampled_points": 0,
            "probe_points": len(self.probes),
        }
        self.probe_values = sample_packets(
            support, self.probes, sample_support, self.work
        )
        self.max_nodes = max_nodes
        self.tags, self.values = [EMPTY], []
        self.unknown_offsets, self.unknown_sources = [0], []
        self.cache = {}

    def samples(self, coordinates):
        flat = coordinates.reshape(-1, 3)
        unique, mapping = np.unique(flat, axis=0, return_inverse=True)
        missing = [tuple(p) for p in unique if tuple(p) not in self.cache]
        if missing:
            points = self.low + np.asarray(missing) * self.unit
            values = sample_packets(self.support, points, sample_support, self.work)
            self.work["sampled_points"] += len(missing)
            self.cache.update(zip(missing, values, strict=True))
        values = np.array([self.cache[tuple(p)] for p in unique])
        return values[mapping].reshape(coordinates.shape[:-1])

    def needs(self, node, candidates):
        at = len(self.unknown_offsets) - 1
        self.tags[node] = UNKNOWN | at
        sources = np.unique(self.owners[candidates])
        self.unknown_sources.extend(sources.tolist())
        self.unknown_offsets.append(len(self.unknown_sources))

    def contained(self, low, side, candidates):
        points = low + CORNERS * side
        for domain in candidates:
            source = self.domain_slots[domain]
            local = (points - self.support.origins[source]) @ self.support.inverses[
                source
            ]
            plane = self.support.planes[domain]
            if (
                local[:, 2].min() < self.support.bottoms[domain]
                or np.max(local[:, 2] - local[:, :2] @ plane[:2] - plane[2]) > 0
            ):
                continue
            projection = shapely.MultiPoint(local[:, :2]).convex_hull
            if shapely.covers(self.support.domains[domain], projection):
                return True
        return False

    def split(self, record, pending):
        node, coordinate, width, level, candidates, probes = record
        first = len(self.tags)
        self.tags[node] = first
        self.tags.extend([EMPTY] * 8)
        middle = self.low + (coordinate + width / 2) * self.unit
        slots = ((self.probes[probes] >= middle) * [4, 2, 1]).sum(1)
        for child, corner in enumerate(CORNERS.astype(int)):
            pending.append(
                (
                    first + child,
                    coordinate + corner * (width // 2),
                    width // 2,
                    level + 1,
                    candidates,
                    probes[slots == child],
                )
            )

    def level(self, records):
        active = []
        for record in records:
            node, coordinate, width, level, candidates, probes = record
            low = self.low + coordinate * self.unit
            high = low + width * self.unit
            self.work["source_box_tests"] += len(candidates)
            overlap = (
                (self.boxes[candidates, 1] >= low) & (self.boxes[candidates, 0] <= high)
            ).all(1)
            candidates = candidates[overlap]
            if len(candidates):
                active.append((node, coordinate, width, level, candidates, probes))
        if not active:
            return []
        coordinates = np.array([c + GRID * w for _, c, w, _, _, _ in active], np.int64)
        samples = self.samples(coordinates)
        pending = []
        for record, sampled in zip(active, samples, strict=True):
            node, coordinate, width, level, candidates, probes = record
            side = width * self.unit
            centre, radius = sampled[13], np.sqrt(3) * side / 2
            if centre - radius > 1e-10:
                continue
            if centre + radius < -1e-10:
                self.tags[node] = SOLID
                continue
            low = self.low + coordinate * self.unit
            values = sampled[CORNER_IDS].astype(np.float32)
            zero = np.all(values == 0)
            if zero and self.contained(low, side, candidates):
                self.tags[node] = SOLID
                continue
            grid_value = evaluate(values, low, low + side, low + GRID * side)[0]
            exterior = (sampled > 0) | (grid_value > 0)
            error = float(
                np.max(abs(grid_value[exterior] - sampled[exterior]), initial=0)
            )
            probe_value = evaluate(values, low, low + side, self.probes[probes])[0]
            truth = self.probe_values[probes]
            certain = abs(truth) > 1e-8
            signs_match = np.all((probe_value[certain] <= 0) == (truth[certain] <= 0))
            reconstructed = (
                not zero
                and values.min() <= 0
                and error <= max(1e-4, side * 0.05)
                and signs_match
            )
            if self.observer is not None:
                self.observer(self, record, sampled, error, probe_value, reconstructed)
            if reconstructed:
                self.tags[node] = LEAF | len(self.values)
                self.values.append(values)
            elif level < self.depth and len(self.tags) + 8 <= self.max_nodes:
                self.split(record, pending)
            else:
                self.needs(node, candidates)
        return pending

    def run(self):
        pending = [
            (
                0,
                np.zeros(3, int),
                self.units,
                0,
                np.arange(len(self.boxes)),
                np.arange(len(self.probes)),
            )
        ]
        while pending:
            pending = self.level(pending)
        return {
            "root_low": self.low,
            "root_side": np.array(self.side),
            "source_bounds": np.stack(
                (self.boxes[:, 0].min(0), self.boxes[:, 1].max(0))
            ),
            "tags": np.array(self.tags, np.uint32),
            "values": np.asarray(self.values, np.float32).reshape(-1, 8),
            "unknown_offsets": np.array(self.unknown_offsets, np.uint32),
            "unknown_sources": np.array(self.unknown_sources, np.uint32),
        }, self.work
