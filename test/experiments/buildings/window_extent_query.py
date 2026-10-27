"""Plan-only closed-window bounds and an exact stored-AABB radius contract."""

import math
from dataclasses import dataclass
from fractions import Fraction

import numpy as np
import shapely

MAX_COORDINATE_M = 1e8
MAX_BOUNDS = 8192


@dataclass(frozen=True)
class WindowBounds:
    identifiers: tuple
    boxes: np.ndarray

    def __post_init__(self):
        if not isinstance(self.identifiers, (tuple, list)) or not isinstance(
            self.boxes, np.ndarray
        ):
            raise TypeError("Bounded ID sequence and NumPy extent array required")
        ids, values = tuple(self.identifiers), self.boxes
        if (
            len(ids) > MAX_BOUNDS
            or any(not isinstance(s, str) or not s or len(s) > 1024 for s in ids)
            or len(set(ids)) != len(ids)
            or values.shape != (len(ids), 2, 3)
            or values.dtype.kind not in "fi"
            or not np.isfinite(values).all()
            or np.any(abs(values) > MAX_COORDINATE_M)
            or np.any(values[:, 0] > values[:, 1])
        ):
            raise ValueError("Bounded finite immutable window extent metadata required")
        object.__setattr__(self, "identifiers", ids)
        owned = np.ascontiguousarray(values, dtype=np.float64)
        object.__setattr__(
            self,
            "boxes",
            np.frombuffer(owned.tobytes(), np.float64).reshape(owned.shape),
        )


def exact_intersects(box, point, radius):
    q = Fraction.from_float
    total = Fraction(0)
    for low, high, value in zip(box[0], box[1], point, strict=True):
        gap = max(
            q(float(low)) - q(float(value)),
            q(float(value)) - q(float(high)),
            Fraction(0),
        )
        total += gap * gap
    return total <= q(float(radius)) ** 2


def positive_sum_squares(values, toward):
    squared = np.where(values == 0, 0, np.nextafter(values * values, toward))
    squared = np.maximum(squared, 0)
    result = squared[:, 0]
    for axis in (1, 2):
        value = result + squared[:, axis]
        result = np.where(value == 0, 0, np.nextafter(value, toward))
        result = np.maximum(result, 0)
    return result


def intersects(boxes, point, radius):
    gaps = np.where(
        point < boxes[:, 0],
        boxes[:, 0] - point,
        np.where(point > boxes[:, 1], point - boxes[:, 1], 0),
    )
    low = np.where(gaps == 0, 0, np.maximum(np.nextafter(gaps, -np.inf), 0))
    high = np.where(gaps == 0, 0, np.nextafter(gaps, np.inf))
    lower = positive_sum_squares(low, -np.inf)
    upper = positive_sum_squares(high, np.inf)
    r2 = radius * radius
    rlo = max(0, float(np.nextafter(r2, -np.inf))) if radius else 0
    rhi = float(np.nextafter(r2, np.inf)) if radius else 0
    accepted, rejected = upper <= rlo, lower > rhi
    uncertain = np.flatnonzero(~(accepted | rejected))
    for i in uncertain:
        accepted[i] = exact_intersects(boxes[i], point, radius)
    return accepted, len(uncertain)


@dataclass(frozen=True)
class RangeSelection:
    identifiers: tuple
    tested_bounds: int
    rational_tests: int


class WindowRange:
    """One optional immutable GEOS broad phase; no residency, LOD or DB policy."""

    def __init__(self, metadata, *, indexed=False):
        if not isinstance(metadata, WindowBounds) or type(indexed) is not bool:
            raise TypeError("WindowBounds and explicit index choice required")
        self.metadata = metadata
        self.tree = None
        self.tree_ids = self.special = np.empty(0, np.int64)
        if indexed:
            boxes = metadata.boxes
            regular = np.all(boxes[:, 0, :2] < boxes[:, 1, :2], axis=1)
            self.tree_ids, self.special = (
                np.flatnonzero(regular),
                np.flatnonzero(~regular),
            )
            selected = boxes[self.tree_ids]
            shapes = shapely.box(
                selected[:, 0, 0],
                selected[:, 0, 1],
                selected[:, 1, 0],
                selected[:, 1, 1],
            )
            self.tree = shapely.STRtree(shapes)

    def query(self, center, radius):
        values = np.asarray(center)
        if (
            values.shape != (3,)
            or values.dtype.kind not in "fi"
            or not np.isfinite(values).all()
            or np.any(abs(values) > MAX_COORDINATE_M)
            or isinstance(radius, (bool, str, np.bool_))
            or not isinstance(radius, (int, float, np.integer, np.floating))
            or not 0 <= radius <= MAX_COORDINATE_M
            or not math.isfinite(radius)
        ):
            raise ValueError(
                "Finite same-frame metre center and bounded radius required"
            )
        point, radius = np.asarray(values, float), float(radius)
        boxes = self.metadata.boxes
        if self.tree is None:
            chosen, exact = intersects(boxes, point, radius)
            selected = np.flatnonzero(chosen)
            tested = len(boxes)
        else:
            low = np.nextafter(point[:2] - radius, -np.inf)
            high = np.nextafter(point[:2] + radius, np.inf)
            region = (
                shapely.Point(point[:2]) if radius == 0 else shapely.box(*low, *high)
            )
            ids = np.sort(np.r_[self.tree_ids[self.tree.query(region)], self.special])
            chosen, exact = intersects(boxes[ids], point, radius)
            selected, tested = ids[chosen], len(ids)
        return RangeSelection(
            tuple(self.metadata.identifiers[i] for i in selected), tested, exact
        )
