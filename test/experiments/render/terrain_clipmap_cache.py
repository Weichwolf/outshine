"""Toroidal height storage: retain overlap and sample only newly exposed bands."""

from dataclasses import dataclass

import numpy as np


def rectangle(low, size):
    x, y = np.meshgrid(np.arange(size[0]), np.arange(size[1]))
    return np.column_stack((x.ravel(), y.ravel())) + low


def exposed_nodes(old, new, side):
    if old is None or np.any(np.abs(new - old) >= side):
        return rectangle(new, (side, side))
    dx, dy = new - old
    columns = rectangle(new + (side - dx if dx > 0 else 0, 0), (abs(dx), side))
    rows = rectangle(new + (max(-dx, 0), side - dy if dy > 0 else 0),
                     (side - abs(dx), abs(dy)))
    return np.concatenate((columns, rows))


@dataclass
class HeightLevel:
    step: float
    values: np.ndarray
    low: np.ndarray | None = None


class HeightClipmap:
    def __init__(self, sample, levels=13, half_cells=64, cell_m=1.0):
        if levels < 1 or half_cells < 8 or half_cells % 2 or not np.isfinite(cell_m) or cell_m <= 0:
            raise ValueError("positive levels/cell size and an even half-grid of at least eight cells required")
        self.sample = sample
        self.half_cells = half_cells
        side = 2 * half_cells + 3
        self.levels = [HeightLevel(cell_m * 2**level, np.full((side, side), np.nan))
                       for level in range(levels)]

    def move(self, camera):
        camera = np.asarray(camera, dtype=np.float64)
        if camera.shape != (2,) or not np.all(np.isfinite(camera)):
            raise ValueError("a finite two-dimensional camera is required")
        updates = []
        for level in self.levels:
            units = camera / (2 * level.step)
            if np.any(np.abs(units) >= np.iinfo(np.int64).max // 2 - len(level.values)):
                raise ValueError("camera exceeds the integer height-cache domain")
            low = np.floor(units).astype(np.int64) * 2 - self.half_cells - 1
            nodes = exposed_nodes(level.low, low, len(level.values))
            updates.append((level, low, nodes))
        count = sum(len(nodes) for _, _, nodes in updates)
        if count:
            points = np.concatenate([nodes * level.step for level, _, nodes in updates])
            values = np.asarray(self.sample(points), dtype=np.float64)
            if values.shape != (count,) or np.any(np.isinf(values)):
                raise ValueError("one finite or explicitly unknown height per node required")
            at = 0
            for level, _, nodes in updates:
                indices = nodes % len(level.values)
                level.values[indices[:, 1], indices[:, 0]] = values[at:at + len(nodes)]
                at += len(nodes)
        for level, low, _ in updates:
            level.low = low
        return count

    def __call__(self, xy):
        xy = np.asarray(xy, dtype=np.float64)
        if xy.ndim != 2 or xy.shape[1] != 2 or not np.all(np.isfinite(xy)):
            raise ValueError("finite local east/north positions required")
        result = np.full(len(xy), np.nan)
        found = np.zeros(len(xy), dtype=bool)
        for level in self.levels:
            if level.low is None:
                continue
            pending = np.flatnonzero(~found)
            units = xy[pending] / level.step
            if np.any(np.abs(units) >= np.iinfo(np.int64).max - len(level.values)):
                raise ValueError("query exceeds the integer height-cache domain")
            nodes = np.rint(units).astype(np.int64)
            aligned = np.all(units == nodes, axis=1)
            inside = aligned & np.all((nodes >= level.low) & (nodes < level.low + len(level.values)), axis=1)
            indices = nodes[inside] % len(level.values)
            result[pending[inside]] = level.values[indices[:, 1], indices[:, 0]]
            found[pending[inside]] = True
        return result
