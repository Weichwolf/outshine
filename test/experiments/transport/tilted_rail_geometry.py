"""Rigid metric-plane adaptation of the existing horizontal rail section kernel."""

from dataclasses import dataclass
from itertools import pairwise

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.transport.rail_hardware import RailRecipe, build_rail_hardware

DEFAULT_RECIPE = RailRecipe()


@dataclass(frozen=True)
class TiltedRail:
    rails: Mesh
    sleeper_transforms: np.ndarray
    footprints: tuple
    sleeper_bottoms: tuple
    plane_origin: np.ndarray
    plane_frame: np.ndarray
    horizontal: object


def plane_frame(triangle):
    p = np.asarray(triangle, float)
    if p.shape != (3, 3) or not np.isfinite(p).all():
        raise ValueError("one explicit finite affine bed triangle required")
    normal = np.cross(p[1] - p[0], p[2] - p[0])
    if normal[2] == 0 or not np.isfinite(normal).all():
        raise ValueError("nondegenerate height plane required")
    normal *= np.sign(normal[2])
    normal /= np.linalg.norm(normal)
    first = np.array([1.0, 0, -normal[0] / normal[2]])
    first /= np.linalg.norm(first)
    return p[0].copy(), np.column_stack((first, np.cross(normal, first), normal))


def transform_points(points, origin, frame, *, method="batch"):
    if method == "batch":
        return points @ frame.T + origin
    if method == "scalar":
        return np.array([frame @ point + origin for point in points])
    raise ValueError("known equivalent rigid transform required")


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


def build_tilted(xy, triangle, *, recipe=DEFAULT_RECIPE, method="batch"):
    origin, frame = plane_frame(triangle)
    xy = np.asarray(xy, float)
    gradient = -frame[:2, 2] / frame[2, 2]
    height = origin[2] + (xy - origin[:2]) @ gradient
    local = (np.c_[xy, height] - origin) @ frame[:, :2]
    horizontal, note = build_rail_hardware(local, 0.0, recipe)
    if horizontal is None:
        return None, note
    rails = Mesh(
        transform_points(horizontal.rails.vertices, origin, frame, method=method),
        horizontal.rails.triangles,
    )
    transforms = np.empty_like(horizontal.sleeper_transforms)
    transforms[:, :, :3] = frame @ horizontal.sleeper_transforms[:, :, :3]
    transforms[:, :, 3] = transform_points(
        horizontal.sleeper_transforms[:, :, 3], origin, frame, method=method
    )
    unit = np.array([[-0.5, -0.5, 0], [0.5, -0.5, 0], [0.5, 0.5, 0], [-0.5, 0.5, 0]])
    bottoms = (
        np.einsum("nij,vj->nvi", transforms[:, :, :3], unit) + transforms[:, None, :, 3]
    )
    sleeper_shapes = [
        shapely.MultiPoint(np.vstack((base, base + transform[:, 2]))[:, :2]).convex_hull
        for base, transform in zip(bottoms, transforms, strict=True)
    ]
    shapes = tuple(projected_rail_footprints(rails)) + tuple(sleeper_shapes)
    transforms.setflags(write=False)
    bottoms.setflags(write=False)
    origin.setflags(write=False)
    frame.setflags(write=False)
    note = dict(
        note,
        bed_gradient=gradient.tolist(),
        frame_determinant=float(np.linalg.det(frame)),
        normal_vertical=frame[2, 2],
        scope="Rigid metric affine bed; whole finite contacts/conflicts are caller-owned. No turnout or C0-crease join.",
    )
    return TiltedRail(
        rails, transforms, shapes, tuple(bottoms), origin, frame, horizontal
    ), note
