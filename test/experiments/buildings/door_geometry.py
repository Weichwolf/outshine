"""Own hinged leaf poses and conservative rotational sweep bounds in a metric XY frame."""

import itertools
import math
from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.contracts import identifier


@dataclass(frozen=True, slots=True)
class DoorLeaf:
    id: str
    centre: tuple[float, float]
    tangent: tuple[float, float]
    inward: tuple[float, float]
    floor_z_m: float
    width_m: float = 1.10
    height_m: float = 2.15
    wall_face_m: float = 0.06
    hinge_side: int = -1
    thickness_m: float = 0.04
    overlap_m: float = 0.025
    stand_off_m: float = 0.012

    def __post_init__(self):
        identifier(self.id)
        for key in ("centre", "tangent", "inward"):
            value = np.asarray(getattr(self, key), dtype=float)
            if value.shape != (2,) or not np.isfinite(value).all():
                raise ValueError("Door frame requires finite XY pairs")
            object.__setattr__(self, key, tuple(float(v) for v in value))
        t, n = np.asarray(self.tangent), np.asarray(self.inward)
        if abs(t @ t - 1) > 1e-9 or abs(n @ n - 1) > 1e-9 or abs(t @ n) > 1e-9:
            raise ValueError("Door tangent and inward axes must be orthonormal")
        dimensions = (
            self.width_m,
            self.height_m,
            self.wall_face_m,
            self.thickness_m,
            self.overlap_m,
            self.stand_off_m,
        )
        if (
            any(isinstance(v, (bool, str)) for v in dimensions)
            or not np.isfinite(dimensions).all()
            or min(dimensions) <= 0
        ):
            raise ValueError("Door dimensions require finite positive metres")
        if (
            not math.isfinite(self.floor_z_m)
            or type(self.hinge_side) is not int
            or self.hinge_side not in (-1, 1)
        ):
            raise ValueError("Door datum or hinge side is invalid")
        if self.height_m <= 0.02 or self.width_m <= 2 * self.thickness_m:
            raise ValueError("Door dimensions leave no usable leaf")

    @property
    def hinge(self):
        return (
            np.asarray(self.centre)
            + self.hinge_side
            * np.asarray(self.tangent)
            * (self.width_m / 2 + self.overlap_m)
            + np.asarray(self.inward)
            * (self.wall_face_m + self.stand_off_m + self.thickness_m / 2)
        )

    @property
    def leaf_width_m(self):
        return self.width_m + 2 * self.overlap_m


def pose(leaf, angle_deg):
    if (
        not isinstance(leaf, DoorLeaf)
        or isinstance(angle_deg, bool)
        or not math.isfinite(angle_deg)
        or not 0 <= angle_deg <= 90
    ):
        raise ValueError("Door pose requires a leaf and an angle in [0, 90] degrees")
    angle = math.radians(angle_deg)
    t, n = np.asarray(leaf.tangent), np.asarray(leaf.inward)
    along = -leaf.hinge_side * t * math.cos(angle) + n * math.sin(angle)
    across = leaf.hinge_side * t * math.sin(angle) + n * math.cos(angle)
    return shapely.Polygon(
        [
            leaf.hinge + along * x + across * y
            for x, y in (
                (0, -leaf.thickness_m / 2),
                (leaf.leaf_width_m, -leaf.thickness_m / 2),
                (leaf.leaf_width_m, leaf.thickness_m / 2),
                (0, leaf.thickness_m / 2),
            )
        ]
    )


def sweep(leaf, error_m=0.001, method="segmented", max_segments=128):
    if not isinstance(leaf, DoorLeaf) or method not in ("single-hull", "segmented"):
        raise ValueError("Unknown door or rotational sweep method")
    if (
        isinstance(error_m, bool)
        or not math.isfinite(error_m)
        or not 1e-8 <= error_m <= 0.01
    ):
        raise ValueError("Door sweep error requires 1e-8..0.01 metres")
    if type(max_segments) is not int or not 1 <= max_segments <= 128:
        raise ValueError("Door sweep segment capacity requires 1..128")
    radius = math.hypot(leaf.leaf_width_m, leaf.thickness_m / 2)
    step = 4 * math.asin(math.sqrt(min(1.0, (error_m / radius) / 2)))
    count = 1 if method == "single-hull" else math.ceil((math.pi / 2) / step)
    if count > max_segments:
        raise ValueError("Door sweep exceeds its conservative segment capacity")
    delta = math.pi / (2 * count)
    bound = 2 * radius * math.sin(delta / 4) ** 2
    angles = np.linspace(0.0, 90.0, count + 1)
    footprints = [pose(leaf, float(a)) for a in angles]
    pieces = [
        shapely.union_all([a, b]).convex_hull.buffer(bound + 1e-10, join_style="mitre")
        for a, b in itertools.pairwise(footprints)
    ]
    result = shapely.union_all(pieces)
    return result, {
        "method": method,
        "segments": count,
        "radius_m": radius,
        "maximum_point_chord_deviation_m": bound,
        "requested_error_m": error_m,
        "scope": "Each moving point lies within the stated radius-based sagitta of its endpoint chord; convex endpoint hull plus that outward margin contains every intermediate leaf pose. No optimal swept boundary or global Hausdorff overcoverage claim.",
    }
