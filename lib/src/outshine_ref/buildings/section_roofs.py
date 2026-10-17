"""Continuous metrical roof sections over bounded polygonal domains with holes."""

from dataclasses import dataclass

import numpy as np
import shapely

from .modelling.scope import scalar


@dataclass(frozen=True, slots=True)
class RoofSection:
    knots: tuple
    axis_xy: tuple = (1.0, 0.0)
    origin_xy: tuple = (0.0, 0.0)

    def __post_init__(self):
        if not isinstance(self.knots, tuple) or not 2 <= len(self.knots) <= 65:
            raise ValueError("Roof section requires 2..65 immutable ordered knots")
        if any(not isinstance(p, tuple) or len(p) != 2 for p in self.knots):
            raise ValueError("Roof knots require immutable (run, height) metre pairs")
        knots = tuple(tuple(scalar(v) for v in p) for p in self.knots)
        if np.any(np.diff(np.asarray(knots)[:, 0]) <= 1e-8):
            raise ValueError("Roof section requires strictly increasing metre runs")
        for name in ("axis_xy", "origin_xy"):
            value = getattr(self, name)
            if not isinstance(value, tuple) or len(value) != 2:
                raise ValueError("Roof frame requires immutable XY pairs")
            object.__setattr__(self, name, tuple(scalar(v) for v in value))
        if abs(np.linalg.norm(self.axis_xy) - 1) > 1e-12:
            raise ValueError("Roof section axis must have unit length")
        object.__setattr__(self, "knots", knots)

    @property
    def axes(self):
        x, y = self.axis_xy
        return np.array([[x, y], [-y, x]])


def prepare(polygon, section, max_triangles):
    if not isinstance(section, RoofSection):
        raise TypeError("A RoofSection is required")
    if (
        not isinstance(polygon, shapely.Polygon)
        or polygon.is_empty
        or not polygon.is_valid
        or polygon.has_z
        or not np.isfinite(shapely.get_coordinates(polygon)).all()
    ):
        raise ValueError("Roof section needs one finite valid planar source polygon")
    count = sum(len(r.coords) - 1 for r in (polygon.exterior, *polygon.interiors))
    if not 3 <= count <= 256:
        raise ValueError("Roof section source exceeds 256 boundary vertices")
    bound = 3 * (count + 2 * len(polygon.interiors) - 2) * (len(section.knots) - 1)
    if type(max_triangles) is not int or max_triangles < bound:
        raise ValueError("Roof section conservative triangle capacity exceeded")
    local = shapely.transform(
        polygon, lambda xy: (xy - section.origin_xy) @ section.axes.T
    )
    lo, _, hi, _ = local.bounds
    knots = np.asarray(section.knots)
    tolerance = max(1e-8, max(abs(lo), abs(hi)) * 1e-12)
    if lo < knots[0, 0] - tolerance or hi > knots[-1, 0] + tolerance:
        raise ValueError("Roof section does not cover its source domain")
    return local, knots


def pack(triangles, owners, section, material, max_triangles):
    if len(triangles) == 0 or len(triangles) > max_triangles:
        raise ValueError("Roof section output exceeds its triangle capacity")
    local = np.asarray(triangles, dtype=float).copy()
    cross = np.cross(
        np.c_[local[:, 1] - local[:, 0], np.zeros(len(local))],
        np.c_[local[:, 2] - local[:, 0], np.zeros(len(local))],
    )[:, 2]
    if np.any(abs(cross) <= 1e-12):
        raise ValueError("Roof section contains an unresolved projected triangle")
    local[cross < 0] = local[cross < 0, ::-1]
    xy = local.reshape(-1, 2)
    knots = np.asarray(section.knots)
    heights = np.interp(xy[:, 0], knots[:, 0], knots[:, 1])
    developed = np.r_[0, np.cumsum(np.linalg.norm(np.diff(knots, axis=0), axis=1))]
    uv = np.c_[np.interp(xy[:, 0], knots[:, 0], developed), xy[:, 1]]
    vertices = np.c_[xy @ section.axes + section.origin_xy, heights]
    points = vertices.reshape(-1, 3, 3)
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    return {
        "vertices": vertices,
        "faces": np.arange(len(vertices), dtype=np.uint32).reshape(-1, 3),
        "normals": np.repeat(normals, 3, axis=0),
        "uv": uv,
        "materials": np.full(len(points), material, dtype=np.uint16),
        "parts": np.asarray(owners, dtype=np.int32),
    }


def section_surface(polygon, section, *, material=9, max_triangles=65536):
    """Open ENU upper surface; part IDs identify successive section intervals."""
    if (
        isinstance(material, bool)
        or not isinstance(material, (int, np.integer))
        or not 0 <= material <= 65535
    ):
        raise ValueError("Roof material requires an unsigned 16-bit identifier")
    local, knots = prepare(polygon, section, max_triangles)
    first, low, last, high = local.bounds
    left, right = knots[:-1, 0].copy(), knots[1:, 0].copy()
    left[0], right[-1] = min(first, left[0]) - 1, max(last, right[-1]) + 1
    strips = shapely.box(left, low - 1, right, high + 1)
    cells = shapely.intersection(local, strips)
    pieces, owners = shapely.get_parts(
        shapely.constrained_delaunay_triangles(cells), return_index=True
    )
    coordinates = shapely.get_coordinates(pieces)
    if len(coordinates) != 4 * len(pieces):
        raise ValueError("Roof section triangulator returned a non-triangle")
    triangles = coordinates.reshape(-1, 4, 2)[:, :3]
    return pack(triangles, owners, section, material, max_triangles)
