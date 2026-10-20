"""Best-first finite-volume normal measures; refinement follows measure variation."""

import heapq
from dataclasses import dataclass

import numpy as np
import shapely


@dataclass
class Cell:
    bounds: np.ndarray
    polygons: np.ndarray
    words: np.ndarray

    def __post_init__(self):
        area = float(np.prod(self.bounds[1] - self.bounds[0]))
        unique, owner = np.unique(self.words, return_inverse=True)
        mass = np.bincount(
            owner, weights=shapely.area(self.polygons), minlength=len(unique)
        )
        if np.any(mass < 0) or mass.sum() > area * (1 + 1e-9):
            raise ValueError(
                f"non-disjoint normal measure: mass={mass.sum()!r}, area={area!r}, bounds={self.bounds.tolist()}"
            )
        keep = mass > 0
        self.normal_words, self.weights = unique[keep], mass[keep] / area
        background = max(0.0, 1 - self.weights.sum())
        self.error = max(
            0.0, 2 * area * (1 - self.weights @ self.weights - background**2)
        )

    def split(self):
        middle = self.bounds.mean(0)
        result = []
        for y in (0, 1):
            for x in (0, 1):
                lo = np.where([x, y], middle, self.bounds[0])
                hi = np.where([x, y], self.bounds[1], middle)
                clipped = shapely.intersection(self.polygons, shapely.box(*lo, *hi))
                keep = shapely.area(clipped) > 0
                result.append(Cell(np.stack((lo, hi)), clipped[keep], self.words[keep]))
        return result


def compile_measure(polygons, words, *, max_array_bytes, fixed_bytes=0):
    """A storage comparison budget is explicit; it is not a device/world quota."""
    root = Cell(
        np.array([[0.0, 0.0], [1.0, 1.0]]),
        np.asarray(polygons, object),
        np.asarray(words, np.uint32),
    )
    cells, children = [root], [-1]
    queue = [(-root.error, 0)]
    leaves = {0: root}
    stored = fixed_bytes + 8 + 8 + 12 * len(root.weights)
    if stored > max_array_bytes:
        raise ValueError("root measure exceeds comparison storage")
    work = {
        "polygon_cell_clips": 0,
        "attempted_splits": 0,
        "accepted_splits": 0,
        "budget_stops": 0,
    }
    while queue:
        _, index = heapq.heappop(queue)
        cell = leaves[index]
        if cell.error == 0 or np.max(cell.bounds[1] - cell.bounds[0]) <= 2**-24:
            continue
        work["attempted_splits"] += 1
        work["polygon_cell_clips"] += 4 * len(cell.polygons)
        parts = cell.split()
        delta = 44 + 12 * (sum(len(c.weights) for c in parts) - len(cell.weights))
        if stored + delta > max_array_bytes:
            work["budget_stops"] += 1
            continue
        children[index] = len(cells)
        del leaves[index]
        for part in parts:
            at = len(cells)
            cells.append(part)
            children.append(-1)
            leaves[at] = part
            heapq.heappush(queue, (-part.error, at))
        cells[index] = None
        stored += delta
        work["accepted_splits"] += 1
    leaf_at = np.full(len(cells), -1, np.int32)
    offsets, normal_words, weights = [0], [], []
    for at, cell in leaves.items():
        leaf_at[at] = len(offsets) - 1
        normal_words.extend(cell.normal_words)
        weights.extend(cell.weights)
        offsets.append(len(weights))
    arrays = {
        "child": np.array(children, np.int32),
        "leaf": leaf_at,
        "offsets": np.array(offsets, np.uint32),
        "normal_words": np.array(normal_words, np.uint32),
        "weights": np.array(weights, np.float64),
    }
    actual = fixed_bytes + sum(a.nbytes for a in arrays.values())
    if actual != stored:
        raise ValueError("measure storage accounting mismatch")
    work.update(
        nodes=len(cells),
        leaves=len(leaves),
        normal_records=len(weights),
        stored_array_bytes=actual,
        remaining_l1_measure_error=sum(c.error for c in leaves.values()),
        scope="greedy joint coverage/normal measure variation; no global optimum or native cost claim",
    )
    return arrays, work


def leaf_bounds(arrays):
    result = np.zeros((len(arrays["offsets"]) - 1, 2, 2))
    stack = [(0, np.array([[0.0, 0.0], [1.0, 1.0]]))]
    while stack:
        node, bounds = stack.pop()
        child = int(arrays["child"][node])
        if child < 0:
            result[arrays["leaf"][node]] = bounds
            continue
        middle = bounds.mean(0)
        for y in (0, 1):
            for x in (0, 1):
                stack.append(
                    (
                        child + 2 * y + x,
                        np.stack(
                            (
                                np.where([x, y], middle, bounds[0]),
                                np.where([x, y], bounds[1], middle),
                            )
                        ),
                    )
                )
    return result
