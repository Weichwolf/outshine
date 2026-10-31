"""Whole side bodies on an explicit finite flat deck; no safety or World authority."""

from dataclasses import dataclass

import numpy as np
import shapely
from foot_ramp_geometry import close_caps
from outshine_ref.contracts import Mesh
from shapely.ops import substring
from stair_module_topology import check_surface
from surface_solid_checks import audit_closed


@dataclass(frozen=True)
class BarrierRecipe:
    height_m: float = 0.9
    thickness_m: float = 0.12
    edge_margin_m: float = 0.05
    end_setback_m: float = 0.05
    clear_half_width_m: float = 1.5

    def __post_init__(self):
        values = (
            self.height_m,
            self.thickness_m,
            self.edge_margin_m,
            self.end_setback_m,
            self.clear_half_width_m,
        )
        if not np.isfinite(values).all() or min(values) <= 0:
            raise ValueError("Finite positive explicit barrier dimensions required")


DEFAULT_RECIPE = BarrierRecipe()


def strip_caps(axis, width_m, recipe):
    axis = np.asarray(axis, float)
    if (
        axis.ndim != 2
        or axis.shape[1] != 2
        or not 2 <= len(axis) <= 256
        or not np.isfinite(axis).all()
        or abs(axis).max() > 8190
        or not np.isfinite(width_m)
        or not 0 < width_m <= 30
    ):
        raise ValueError("Bounded finite complete metric source chain required")
    delta = np.diff(axis, axis=0)
    lengths = np.linalg.norm(delta, axis=1)
    if np.any(lengths <= 1e-6):
        raise ValueError("Whole chain has a zero length segment")
    line = shapely.LineString(axis)
    if not line.is_simple or line.is_closed:
        raise ValueError("Open nonintersecting whole chain required")
    if line.length <= 2 * recipe.end_setback_m:
        raise ValueError(
            "Whole side chain is shorter than its two declared end setbacks"
        )
    tangent = delta / lengths[:, None]
    if np.any(
        np.einsum("ij,ij->i", tangent[:-1], tangent[1:]) < np.cos(np.deg2rad(10))
    ):
        raise ValueError("Barrier corner exceeds existing rail recipe domain")
    outer = width_m / 2 - recipe.edge_margin_m
    inner = outer - recipe.thickness_m
    if inner < recipe.clear_half_width_m:
        raise ValueError("Side bodies consume the declared central clear strip")
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    miter = np.vstack((normal[0], (normal[:-1] + normal[1:]) / 2, normal[-1]))
    miter[1:-1] /= np.einsum("ij,ij->i", miter[1:-1], normal[:-1])[:, None]
    advance = lengths - outer * abs(
        np.einsum("ij,ij->i", np.diff(miter, axis=0), tangent)
    )
    if np.any(advance <= 1e-6):
        raise ValueError("Side offset folds before the next source station")
    line = substring(line, recipe.end_setback_m, line.length - recipe.end_setback_m)
    axis = np.asarray(line.coords)
    delta = np.diff(axis, axis=0)
    tangent = delta / np.linalg.norm(delta, axis=1)[:, None]
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    miter = np.vstack((normal[0], (normal[:-1] + normal[1:]) / 2, normal[-1]))
    miter[1:-1] /= np.einsum("ij,ij->i", miter[1:-1], normal[:-1])[:, None]
    i = 2 * np.arange(len(axis) - 1, dtype=np.uint32)
    faces = np.stack(
        (np.c_[i, i + 2, i + 1], np.c_[i + 1, i + 2, i + 3]), axis=1
    ).reshape(-1, 3)
    result = []
    for side in (-1, 1):
        xy = (
            axis[:, None, :]
            + side * np.array([inner, outer])[None, :, None] * miter[:, None, :]
        ).reshape(-1, 2)
        triangles = faces[:, ::-1] if side < 0 else faces
        check_surface(xy, triangles)
        polygon = shapely.Polygon(np.vstack((xy[::2], xy[1::2][::-1])))
        if not polygon.is_valid or polygon.is_empty or len(polygon.interiors):
            raise ValueError("Two complete unfolded side strips required")
        upper = Mesh(np.c_[xy, np.full(len(xy), recipe.height_m)], triangles)
        lower = Mesh(np.c_[xy, np.zeros(len(xy))], triangles)
        result.append((polygon, upper, lower))
    if not result[0][0].disjoint(result[1][0]):
        raise ValueError("Opposing whole side bodies intersect")
    return tuple(result)


def construct(axis, width_m, bed, clearance_m, recipe=DEFAULT_RECIPE):
    if (
        not isinstance(bed, (shapely.Polygon, shapely.MultiPolygon))
        or bed.is_empty
        or not bed.is_valid
        or not np.isfinite(clearance_m)
        or recipe.height_m > clearance_m
    ):
        raise ValueError(
            "Finite complete held bed and sufficient reserved height required"
        )
    parts = strip_caps(axis, width_m, recipe)
    if not all(bed.covers(p) for p, _upper, _lower in parts):
        raise ValueError("Whole side strips lack finite deck support")
    result = []
    for polygon, upper, lower in parts:
        body = close_caps(upper, lower)
        proof = []
        for cast in (np.float64, np.float32):
            stored = Mesh(body.vertices.astype(cast).astype(float), body.triangles)
            floor = lower.vertices.astype(cast).astype(float)
            shape = shapely.coverage_union_all(
                shapely.polygons(floor[lower.triangles, :2])
            )
            check_surface(floor[:, :2], lower.triangles)
            volume = audit_closed(stored)
            if volume <= 0 or not bed.covers(shape):
                raise ValueError("Stored whole side body loses support or orientation")
            if (
                stored.vertices[:, 2].min() != 0
                or stored.vertices[:, 2].max() > clearance_m
            ):
                raise ValueError("Stored whole side body leaves held reserved height")
            proof.append(
                {
                    "storage": np.dtype(cast).name,
                    "volume_m3": volume,
                    "whole_flat_bed_supported": True,
                }
            )
        result.append(
            {
                "body": body,
                "upper": upper,
                "lower": lower,
                "domain": polygon,
                "proof": proof,
            }
        )
    return tuple(result)
