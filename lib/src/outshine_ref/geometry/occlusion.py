"""Conservative convex opaque-patch umbras over an entire observer box.

Opacity and planar coverage are caller guarantees, never inferred from source
bounds or a surface-distance estimate. Unknown patches generate no occlusion.
"""

import itertools
import math
from dataclasses import dataclass
from fractions import Fraction

import numpy as np


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _sub(a, b):
    return tuple(x - y for x, y in zip(a, b, strict=True))


def _dot(a, b):
    return sum(x * y for x, y in zip(a, b, strict=True))


def _bounded_boxes(boxes):
    boxes = np.asarray(boxes, dtype=np.float64)
    if (
        boxes.ndim != 3
        or boxes.shape[1:] != (2, 3)
        or not np.isfinite(boxes).all()
        or (boxes[:, 0] > boxes[:, 1]).any()
        or np.abs(boxes).max(initial=0) > 1e7
    ):
        raise ValueError(
            "finite ordered 3D boxes within the explicit local coordinate domain required"
        )
    return boxes


def _coefficient_intervals(planes):
    low, high = [], []
    for plane in planes:
        largest = max(abs(x) for x in plane[:3])
        exponent = largest.numerator.bit_length() - largest.denominator.bit_length() + 1
        scale = Fraction(1, 2**exponent) if exponent >= 0 else Fraction(2**-exponent)
        lo, hi = [], []
        for coefficient in plane:
            exact = coefficient * scale
            rounded = float(exact)
            rational = Fraction.from_float(rounded)
            lo.append(rounded if rational <= exact else np.nextafter(rounded, -np.inf))
            hi.append(rounded if rational >= exact else np.nextafter(rounded, np.inf))
        low.append(lo)
        high.append(hi)
    return np.asarray(low, dtype=np.float64).reshape(-1, 4), np.asarray(
        high, dtype=np.float64
    ).reshape(-1, 4)


@dataclass(frozen=True, slots=True)
class OpaqueUmbra:
    """Coefficient intervals for halfspaces; caller-owned arrays are copied."""

    low: np.ndarray
    high: np.ndarray
    observer_box: np.ndarray | None = None
    patch_bounds: np.ndarray | None = None

    def __post_init__(self):
        low, high = (
            np.array(self.low, dtype=np.float64, copy=True),
            np.array(self.high, dtype=np.float64, copy=True),
        )
        if (
            low.ndim != 2
            or low.shape[1:] != (4,)
            or high.shape != low.shape
            or len(low) > 129
            or not np.isfinite(low).all()
            or not np.isfinite(high).all()
            or (low > high).any()
        ):
            raise ValueError(
                "bounded finite ordered halfspace coefficient intervals required"
            )
        if len(low) and (np.abs(np.c_[low[:, :3], high[:, :3]]).max(axis=1) == 0).any():
            raise ValueError("halfspaces need nonzero normals")
        low.setflags(write=False)
        high.setflags(write=False)
        object.__setattr__(self, "low", low)
        object.__setattr__(self, "high", high)
        if self.observer_box is not None:
            box = _bounded_boxes(np.asarray(self.observer_box)[None])[0].copy()
            box.setflags(write=False)
            object.__setattr__(self, "observer_box", box)
        elif len(low):
            raise ValueError(
                "a nonempty occlusion certificate must own its observer region"
            )

        if self.patch_bounds is not None:
            bounds = _bounded_boxes(np.asarray(self.patch_bounds)[None])[0].copy()
            bounds.setflags(write=False)
            object.__setattr__(self, "patch_bounds", bounds)
        elif len(low):
            raise ValueError("a nonempty certificate must retain its patch bounds")

    def contains_observer(self, eye):
        eye = np.asarray(eye, dtype=np.float64)
        return (
            self.observer_box is not None
            and eye.shape == (3,)
            and np.isfinite(eye).all()
            and bool(
                ((eye >= self.observer_box[0]) & (eye <= self.observer_box[1])).all()
            )
        )


def convex_patch_umbra(
    vertices, observer_box, *, certified_opaque=False, two_sided=False
):
    """Compile exact halfspaces from represented coordinates, with outward FP intervals.

    Winding defines the front side. Only convex, exactly planar contours without
    holes are supported; unsupported geometry must remain a visible candidate.
    """
    if type(certified_opaque) is not bool or type(two_sided) is not bool:
        raise ValueError("explicit opacity and sidedness booleans required")
    if not certified_opaque:
        return OpaqueUmbra(np.empty((0, 4)), np.empty((0, 4)))
    box = _bounded_boxes(np.asarray(observer_box)[None])[0]
    vertices = np.asarray(vertices, dtype=np.float64)
    if (
        vertices.ndim != 2
        or vertices.shape[1:] != (3,)
        or not 3 <= len(vertices) <= 17
        or not np.isfinite(vertices).all()
        or np.abs(vertices).max() > 1e7
    ):
        raise ValueError("bounded finite polygon contour required")
    if np.array_equal(vertices[0], vertices[-1]):
        vertices = vertices[:-1]
    points = [tuple(Fraction.from_float(float(x)) for x in v) for v in vertices]
    if not 3 <= len(points) <= 16 or len(set(points)) != len(points):
        raise ValueError("3..16 distinct contour vertices required")
    normal = next(
        (
            n
            for i in range(1, len(points) - 1)
            if any(
                n := _cross(_sub(points[i], points[0]), _sub(points[i + 1], points[0]))
            )
        ),
        None,
    )
    if normal is None or any(_dot(normal, _sub(v, points[0])) != 0 for v in points):
        raise ValueError("exactly planar nondegenerate opaque contour required")
    edges = list(zip(points, points[1:] + points[:1], strict=True))
    if any(
        _dot(normal, _cross(_sub(b, a), _sub(v, a))) < 0
        for a, b in edges
        for v in points
    ):
        raise ValueError("a single convex contour without holes required")
    eyes = [
        tuple(Fraction.from_float(float(x)) for x in eye)
        for eye in set(itertools.product(*zip(box[0], box[1], strict=True)))
    ]
    side = [_dot(normal, _sub(e, points[0])) for e in eyes]
    if two_sided and all(s < 0 for s in side):
        normal = tuple(-v for v in normal)
        side = [-s for s in side]
    if not all(s > 0 for s in side):
        return OpaqueUmbra(np.empty((0, 4)), np.empty((0, 4)))
    planes = [(*(-v for v in normal), _dot(normal, points[0]))]
    centre = tuple(sum(v[axis] for v in points) / len(points) for axis in range(3))
    for a, b in edges:
        for eye in eyes:
            n = _cross(_sub(b, a), _sub(eye, a))
            if _dot(n, _sub(centre, a)) < 0:
                n = tuple(-v for v in n)
            planes.append((*n, -_dot(n, a)))
    return OpaqueUmbra(
        *_coefficient_intervals(planes),
        observer_box=box,
        patch_bounds=np.array([vertices.min(axis=0), vertices.max(axis=0)]),
    )


def classify_boxes(boxes, umbra, *, margin_m=1e-6, observer=None):
    """Return fully hidden / fully outside masks using outward interval arithmetic.

    At most 512 boxes are evaluated per call. A nonnegative world-space margin
    loses culling, not coverage. Empty halfspaces hide nothing.
    """
    boxes = _bounded_boxes(boxes)
    if (
        len(boxes) > 512
        or not isinstance(umbra, OpaqueUmbra)
        or not math.isfinite(margin_m)
        or margin_m < 0
    ):
        raise ValueError(
            "bounded box batch, explicit umbra and nonnegative margin required"
        )
    if not len(umbra.low) or (
        observer is not None and not umbra.contains_observer(observer)
    ):
        return np.zeros(len(boxes), dtype=bool), np.zeros(len(boxes), dtype=bool)
    low = np.broadcast_to(umbra.low[:, 3], (len(boxes), len(umbra.low))).copy()
    high = np.broadcast_to(umbra.high[:, 3], low.shape).copy()
    for axis in range(3):
        a, b = boxes[:, 0, axis, None], boxes[:, 1, axis, None]
        c, d = umbra.low[None, :, axis], umbra.high[None, :, axis]
        products = (a * c, a * d, b * c, b * d)
        below = np.nextafter(np.minimum.reduce(products), -np.inf)
        above = np.nextafter(np.maximum.reduce(products), np.inf)
        low = np.nextafter(low + below, -np.inf)
        high = np.nextafter(high + above, np.inf)
    norm_bound = np.maximum(np.abs(umbra.low[:, :3]), np.abs(umbra.high[:, :3]))
    norm_bound = np.nextafter(
        np.nextafter(norm_bound[:, 0] + norm_bound[:, 1], np.inf) + norm_bound[:, 2],
        np.inf,
    )
    threshold = np.nextafter(margin_m * norm_bound, np.inf)
    return (low > threshold).all(axis=1), (high < -threshold).any(axis=1)
