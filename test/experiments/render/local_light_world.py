"""Camera-independent sparse finite-light cells, with exact world IDs (#448)."""

from dataclasses import dataclass
from itertools import product

import numpy as np

POSITION_CELL_M = 128


def encoded_key(cell):
    return tuple((int(axis) & 0xFFFFFFFF) ^ 0x80000000 for axis in cell)


def hashed(key):
    # Fixed experimental odd mixers; collisions are retained and measured.
    return (
        (key[0] * 0x9E3779B1) ^ (key[1] * 0x85EBCA77) ^ (key[2] * 0xC2B2AE3D)
    ) & 0xFFFFFFFF


def cell_positions(positions, width):
    positions = np.asarray(positions, float)
    cells = np.floor(positions / width)
    if (
        not np.all(np.isfinite(cells))
        or np.any(cells < -(2**30))
        or np.any(cells >= 2**30)
    ):
        raise ValueError("private signed cell-coordinate domain exceeded")
    cells = cells.astype("<i4")
    return cells, (positions - cells.astype(float) * width).astype("<f4")


@dataclass
class WorldGrid:
    width: float
    cells: dict
    globals: np.ndarray
    tests: int
    overflow: bool
    local_lights: np.ndarray
    light_cells: np.ndarray
    reason: str | None = None

    def pack(self, method):
        if method not in ("hash", "sorted"):
            raise ValueError("hash or sorted lookup required")
        keys = sorted(self.cells)
        capacity = (
            (1 << max(1, (2 * len(keys) - 1).bit_length()))
            if method == "hash"
            else len(keys)
        )
        slots = np.zeros((max(1, capacity), 5), dtype="<u4")
        indices, insertion = [], []
        for ordinal, cell in enumerate(keys):
            key = encoded_key(cell)
            at = hashed(key) & (capacity - 1) if method == "hash" else ordinal
            probes = 1
            while slots[at, 4]:
                at = (at + 1) & (capacity - 1)
                probes += 1
            insertion.append(probes)
            ids = self.cells[cell]
            slots[at] = [*key, 5 + slots.size + len(indices), len(ids)]
            indices.extend(ids)
        global_at = 5 + slots.size + len(indices)
        pose_at = global_at + len(self.globals) + self.light_cells.size
        words = np.r_[
            np.array(
                [
                    global_at,
                    len(self.globals),
                    5,
                    capacity - 1 if method == "hash" else len(keys),
                    global_at + len(self.globals),
                ],
                dtype="<u4",
            ),
            slots.ravel(),
            np.asarray(indices, dtype="<u4"),
            self.globals,
            self.light_cells.astype("<i4").view("<u4").ravel(),
            np.zeros(self.light_cells.size, dtype="<u4"),
        ].astype("<u4")
        return words, {
            "method": method,
            "cell_width_m": self.width,
            "occupied_cells": len(keys),
            "slot_capacity": len(slots),
            "slot_bytes": slots.nbytes,
            "index_bytes": len(indices) * 4,
            "global_bytes": self.globals.nbytes,
            "header_bytes": 20,
            "coordinate_bytes": self.light_cells.nbytes,
            "position_cell_m": POSITION_CELL_M,
            "pose_bytes": self.light_cells.nbytes,
            "pose_word_offset": pose_at,
            "light_bytes": self.local_lights.nbytes,
            "payload_bytes": words.nbytes,
            "assignments": len(indices),
            "sphere_box_tests": self.tests,
            "insertion_probes_max": max(insertion, default=0),
            "overflow_full_list": self.overflow,
            "fallback_reason": self.reason,
        }

    def candidates(self, points):
        for point in points:
            key = tuple(np.floor(np.asarray(point) / self.width).astype(np.int64))
            yield np.r_[self.globals, self.cells.get(key, ())].astype("<u4")


def build_world(packed, width=32, index_bytes=16 * 1024**2, translation=None):
    if width <= 0 or not np.isfinite(width) or index_bytes <= 0:
        raise ValueError("finite positive cell width and index budget required")
    packed = np.asarray(packed)
    if (
        packed.ndim != 3
        or packed.shape[1:] != (4, 4)
        or not np.all(np.isfinite(packed))
    ):
        raise ValueError("complete finite Light64 records required")
    if np.any(packed[:, 1, 3] < 0) or not np.all(np.isin(packed[:, 0, 3], [0, 1, 2])):
        raise ValueError("declared light kind and nonnegative inverse range required")
    translation = np.zeros(3) if translation is None else np.asarray(translation, float)
    if translation.shape != (3,) or not np.all(np.isfinite(translation)):
        raise ValueError("finite world translation required")
    positions = packed[:, 1, :3].astype(float) + translation
    light_cells, local_positions = cell_positions(positions, POSITION_CELL_M)
    local_lights = packed.copy()
    local_lights[:, 1, :3] = local_positions
    globals_ = np.flatnonzero((packed[:, 0, 3] == 0) | (packed[:, 1, 3] == 0)).astype(
        "<u4"
    )
    finite = np.flatnonzero((packed[:, 0, 3] != 0) & (packed[:, 1, 3] > 0))
    cells, total, tested = {}, 0, 0
    for light in finite:
        centre = positions[light]
        radius = 1.0 / float(packed[light, 1, 3])
        first = np.floor(np.nextafter(centre - radius, -np.inf) / width)
        last = np.floor(np.nextafter(centre + radius, np.inf) / width)
        if np.any(first < -(2**31)) or np.any(last >= 2**31):
            return WorldGrid(
                width,
                {},
                np.arange(len(packed), dtype="<u4"),
                tested,
                True,
                local_lights,
                light_cells,
                "coordinate-range",
            )
        first, last = first.astype(np.int64), last.astype(np.int64)
        shape = last - first + 1
        if np.prod(shape.astype(float)) > 1_000_000:
            return WorldGrid(
                width,
                {},
                np.arange(len(packed), dtype="<u4"),
                tested,
                True,
                local_lights,
                light_cells,
                "cell-work-bound",
            )
        xyz = np.asarray(
            list(product(*(range(a, b + 1) for a, b in zip(first, last)))),
            dtype=np.int64,
        )
        low, high = xyz * width, (xyz + 1) * width
        distance = np.maximum(np.maximum(low - centre, centre - high), 0)
        selected = xyz[np.einsum("ij,ij->i", distance, distance) <= radius * radius]
        tested += len(xyz)
        total += len(selected)
        if total * 4 > index_bytes:
            return WorldGrid(
                width,
                {},
                np.arange(len(packed), dtype="<u4"),
                tested,
                True,
                local_lights,
                light_cells,
                "index-budget",
            )
        for cell in selected:
            cells.setdefault(tuple(cell), []).append(int(light))
    return WorldGrid(width, cells, globals_, tested, False, local_lights, light_cells)


def query_words(words, points, width, method):
    if method not in ("hash", "sorted"):
        raise ValueError("hash or sorted lookup required")
    result, work = [], []
    global_ids = words[words[0] : words[0] + words[1]]
    for point in points:
        key = encoded_key(np.floor(np.asarray(point) / width).astype(np.int64))
        low, high, found, probes = 0, int(words[3]), None, 0
        if method == "hash":
            low, high = hashed(key) & high, high + 1
        while True:
            if method == "sorted" and low >= high:
                break
            if method == "hash" and probes >= high:
                break
            at = (
                (low + high) // 2
                if method == "sorted"
                else (low + probes) & int(words[3])
            )
            slot = words[int(words[2]) + at * 5 : int(words[2]) + at * 5 + 5]
            probes += 1
            if method == "hash" and slot[4] == 0:
                break
            other = tuple(map(int, slot[:3]))
            if key == other:
                found = slot
                break
            if method == "sorted":
                if other < key:
                    low = at + 1
                else:
                    high = at
        local = words[found[3] : found[3] + found[4]] if found is not None else []
        result.append(np.r_[global_ids, local].astype("<u4"))
        work.append(probes)
    return result, np.asarray(work)
