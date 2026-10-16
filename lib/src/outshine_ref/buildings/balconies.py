"""Bounded balcony deck and solid U-shaped guard in local ENU metres, facing −Y."""

import math
from dataclasses import dataclass

import numpy as np
import shapely

from .modelling.prism_union import PrismVolume, primitive_mesh, union_prisms


@dataclass(frozen=True, slots=True)
class BalconyPlan:
    width_m: float
    depth_m: float = 1.2
    back_m: float = 0.24
    slab_m: float = 0.16
    guard_m: float = 0.10
    guard_height_m: float = 1.10

    def __post_init__(self):
        values = (
            self.width_m,
            self.depth_m,
            self.back_m,
            self.slab_m,
            self.guard_m,
            self.guard_height_m,
        )
        if any(
            isinstance(v, (bool, str, np.bool_)) or not math.isfinite(v) or v <= 0
            for v in values
        ):
            raise ValueError("Balcony dimensions require finite positive metres")
        if self.width_m <= 2 * self.guard_m or self.depth_m <= self.guard_m:
            raise ValueError("Balcony guard leaves no positive inner deck")
        left, right = -self.width_m / 2, self.width_m / 2
        if (
            not left < left + self.guard_m < right - self.guard_m < right
            or not -self.depth_m < -self.depth_m + self.guard_m < 0
        ):
            raise ValueError(
                "Balcony guard is unresolved at the supplied coordinate scale"
            )
        if not math.isfinite(self.volume_m3) or self.volume_m3 <= 0:
            raise ValueError("Balcony volume is unresolved or nonfinite")

    @property
    def volume_m3(self):
        slab = self.width_m * (self.depth_m + self.back_m) * self.slab_m
        guard = self.width_m * self.depth_m - (self.width_m - 2 * self.guard_m) * (
            self.depth_m - self.guard_m
        )
        return slab + guard * self.guard_height_m


def volumes(plan):
    if not isinstance(plan, BalconyPlan):
        raise TypeError("BalconyPlan required")
    left, right, depth, g = (
        -plan.width_m / 2,
        plan.width_m / 2,
        plan.depth_m,
        plan.guard_m,
    )
    return (
        PrismVolume(
            "deck", shapely.box(left, -depth, right, plan.back_m), -plan.slab_m, 0.0, 0
        ),
        PrismVolume(
            "guard-front",
            shapely.box(left, -depth, right, -depth + g),
            0.0,
            plan.guard_height_m,
            1,
        ),
        PrismVolume(
            "guard-left",
            shapely.box(left, -depth + g, left + g, 0.0),
            0.0,
            plan.guard_height_m,
            1,
        ),
        PrismVolume(
            "guard-right",
            shapely.box(right - g, -depth + g, right, 0.0),
            0.0,
            plan.guard_height_m,
            1,
        ),
    )


def balcony_surface(plan, *, method="boundary", max_triangles=128):
    """Owned positions/faces/roles. Boundary omits contacts; union/parts are cost controls.

    Role0 is deck and role1 guard. Deck surface is at Z0, attached back at Y>0;
    the opening towards the building is at Y0. Caller owns UVs, materials,
    attachment, collision admission and demand. Limits bound cardinality, not backend time.
    """
    if not isinstance(plan, BalconyPlan):
        raise TypeError("BalconyPlan required")
    if type(max_triangles) is not int or max_triangles < 48:
        raise ValueError("Four-prism balcony input needs at least48 triangle capacity")
    if method not in ("boundary", "union", "parts"):
        raise ValueError("Declared balcony construction method required")
    if method == "boundary":
        from .balcony_boundary import boundary

        return boundary(plan)
    sources = volumes(plan)
    if method == "union":
        result = union_prisms(sources, max_triangles=max_triangles)
        return {
            "vertices": result.mesh["vertices"].copy(),
            "faces": result.mesh["faces"].astype(np.uint32),
            "roles": result.mesh["materials"].astype(np.uint8),
        }
    points, faces, roles = [], [], []
    offset = 0
    for source in sources:
        vertices, indices = primitive_mesh(source)
        points.append(vertices)
        faces.append(indices + offset)
        roles.append(np.full(len(indices), source.material, dtype=np.uint8))
        offset += len(vertices)
    return {
        "vertices": np.concatenate(points),
        "faces": np.concatenate(faces).astype(np.uint32),
        "roles": np.concatenate(roles),
    }
