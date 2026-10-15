"""Rigid metric adaptation of the bounded horizontal rail construction."""

from itertools import pairwise

import numpy as np
import shapely

from ...contracts import Mesh
from ._build import build_rail_hardware
from ._planar_types import PlanarRailHardware
from ._types import RailBudget, RailRecipe

DEFAULT_RECIPE = RailRecipe()
DEFAULT_BUDGET = RailBudget()


def rail_bed_frame(triangle):
    """Return a metric frame of an explicit plane; no finite-bed selection."""
    p = np.asarray(triangle, float)
    if p.shape != (3, 3) or not np.isfinite(p).all():
        raise ValueError("one explicit finite affine bed triangle required")
    with np.errstate(over="ignore", invalid="ignore", divide="ignore", under="ignore"):
        normal = np.cross(p[1] - p[0], p[2] - p[0])
        length = np.linalg.norm(normal)
        if normal[2] == 0 or not np.isfinite(length) or length <= 0:
            raise ValueError("representable nondegenerate height plane required")
        normal *= np.sign(normal[2])
        normal /= length
        first = np.array([1.0, 0, -normal[0] / normal[2]])
        first /= np.linalg.norm(first)
        frame = np.column_stack((first, np.cross(normal, first), normal))
    if not np.isfinite(frame).all() or np.max(abs(frame.T @ frame - np.eye(3))) > 1e-12:
        raise ValueError("representable orthonormal height plane required")
    origin = p[0].copy()
    origin.setflags(write=False)
    frame.setflags(write=False)
    return origin, frame


def projected_rail_footprints(rails):
    rings = rails.vertices.reshape(2, -1, 12, 3)
    shapes = []
    for rail in rings:
        hulls = [
            shapely.MultiPoint(np.vstack((a, b))[:, :2]).convex_hull
            for a, b in pairwise(rail)
        ]
        shape = shapely.union_all(hulls)
        if shape.is_empty or not shape.is_valid or shape.geom_type != "Polygon":
            raise ValueError("rigid rail projection is not a simple finite polygon")
        shapes.append(shape)
    return shapes


def build_planar_rail_hardware(
    xy, triangle, recipe=DEFAULT_RECIPE, budget=DEFAULT_BUDGET
):
    """Construct on one selected infinite affine plane within RailBudget.

    Return (None, reason) for bounded/unplanned hardware. Invalid/unrepresentable
    inputs raise ValueError. Caller verifies whole finite bed and all conflicts.
    """
    if not isinstance(recipe, RailRecipe) or not isinstance(budget, RailBudget):
        raise TypeError("explicit RailRecipe and RailBudget required")
    try:
        count = len(xy)
    except TypeError:
        raise ValueError("bounded metric Nx2 centreline required") from None
    if count > budget.maximum_stations or count * 24 > budget.maximum_rail_vertices:
        return None, {"valid": False, "reason": "rail_station_budget"}
    origin, frame = rail_bed_frame(triangle)
    xy = np.asarray(xy, float)
    if xy.ndim != 2 or xy.shape[1:] != (2,) or len(xy) < 2 or not np.isfinite(xy).all():
        raise ValueError("finite metric Nx2 centreline required")
    with np.errstate(over="ignore", invalid="ignore", divide="ignore"):
        gradient = -frame[:2, 2] / frame[2, 2]
        height = origin[2] + (xy - origin[:2]) @ gradient
        local = (np.c_[xy, height] - origin) @ frame[:, :2]
    if not np.isfinite(local).all():
        raise ValueError("planar rail centreline cannot be represented")
    horizontal, note = build_rail_hardware(local, 0.0, recipe, budget)
    if horizontal is None:
        return None, note
    rails = Mesh(
        horizontal.rails.vertices @ frame.T + origin, horizontal.rails.triangles
    )
    transforms = np.empty_like(horizontal.sleeper_transforms)
    transforms[:, :, :3] = frame @ horizontal.sleeper_transforms[:, :, :3]
    transforms[:, :, 3] = horizontal.sleeper_transforms[:, :, 3] @ frame.T + origin
    unit = np.array([[-0.5, -0.5, 0], [0.5, -0.5, 0], [0.5, 0.5, 0], [-0.5, 0.5, 0]])
    bottoms = (
        np.einsum("nij,vj->nvi", transforms[:, :, :3], unit) + transforms[:, None, :, 3]
    )
    sleeper_shapes = [
        shapely.MultiPoint(np.vstack((base, base + transform[:, 2]))[:, :2]).convex_hull
        for base, transform in zip(bottoms, transforms, strict=True)
    ]
    shapes = tuple(projected_rail_footprints(rails)) + tuple(sleeper_shapes)
    product = PlanarRailHardware(
        rails, transforms, shapes, bottoms, origin, frame, horizontal
    )
    return product, dict(
        note,
        bed_gradient=gradient.tolist(),
        frame_determinant=float(np.linalg.det(frame)),
        normal_vertical=frame[2, 2],
        contact_scope="Explicit affine plane only; caller verifies all finite bed contacts and conflicts.",
        scope="Rigid metric affine bed; whole finite contacts/conflicts are caller-owned. No turnout or C0-crease join.",
    )
