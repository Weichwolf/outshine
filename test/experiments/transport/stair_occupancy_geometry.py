"""Experimental affine clearance queries extending the shared constant-band query."""

from dataclasses import dataclass, field
from fractions import Fraction
from math import isfinite, nextafter

import shapely
from outshine_ref.contracts import HeightInterval, Reservation, geometry


class OccupancyError(ValueError):
    """A local query cannot prove free space and must be conservatively deferred."""


def rational(value):
    return value if isinstance(value, Fraction) else Fraction(float(value))


def plane_value(plane, origin, xy):
    return rational(plane[0]) + sum(
        rational(g) * (rational(x) - rational(o))
        for g, x, o in zip(plane[1:], xy, origin, strict=True)
    )


def bounded_tuple(value, size, bound):
    if (
        not isinstance(value, tuple)
        or len(value) != size
        or any(
            type(v) not in (int, float) or not isfinite(v) or abs(v) > bound
            for v in value
        )
    ):
        raise ValueError("finite immutable bounded affine coordinates required")


def clip_chain(vertices, values):
    """Clip an oriented chain; a nonconvex result may have zero-width connectors."""
    if max(values) <= 0:
        return []
    if min(values) >= 0:
        return vertices
    result = []
    for i, end in enumerate(vertices):
        start, a, b = vertices[i - 1], values[i - 1], values[i]
        if (a < 0 < b) or (b < 0 < a):
            t = a / (a - b)
            result.append(
                tuple(x + t * (y - x) for x, y in zip(start, end, strict=True))
            )
        if b >= 0:
            result.append(end)
    return result


def polygonal_conflict(result):
    if result.is_empty:
        return shapely.Polygon()
    if result.geom_type in ("Polygon", "MultiPolygon"):
        return result
    pending, polygons = [result], []
    visited = 0
    while pending:
        part = pending.pop()
        visited += 1
        if visited > 16384:
            raise OccupancyError("polygonal normalization part budget exhausted")
        if part.is_empty:
            continue
        if part.geom_type == "Polygon":
            polygons.append(part)
        elif part.geom_type in ("MultiPolygon", "GeometryCollection"):
            pending.extend(part.geoms)
    if not polygons:
        return shapely.Polygon()
    result = shapely.union_all(polygons)
    if (
        result.is_empty
        or not result.is_valid
        or result.geom_type not in ("Polygon", "MultiPolygon")
    ):
        raise OccupancyError("polygonal normalization is numerically unresolved")
    return result


@dataclass(frozen=True, slots=True)
class AffineOccupancy(Reservation):
    """Prototype only: height envelope is broad phase; XY conflict is height-dependent."""

    heights: HeightInterval = field(init=False)
    origin_xy_m: tuple[float, float]
    lower_plane_m: tuple[float, float, float]
    upper_plane_m: tuple[float, float, float]
    _whole_overlap_m: tuple[float, float] = field(init=False, repr=False)

    def __post_init__(self):
        geometry(self.geometry, polygonal=True)
        bounded_tuple(self.origin_xy_m, 2, 1e6)
        for plane in (self.lower_plane_m, self.upper_plane_m):
            bounded_tuple(plane, 3, 1e6)
            if max(abs(v) for v in plane[1:]) > 1e4:
                raise ValueError("bounded affine gradients required")
        xy = shapely.get_coordinates(self.geometry)
        if len(xy) > 65536 or abs(xy).max() > 1e6:
            raise ValueError("bounded local affine domain required")
        lower = [plane_value(self.lower_plane_m, self.origin_xy_m, p) for p in xy]
        upper = [plane_value(self.upper_plane_m, self.origin_xy_m, p) for p in xy]
        if any(a >= b for a, b in zip(lower, upper, strict=True)):
            raise ValueError("non-inverting positive affine volume required")
        # An outward-rounded envelope cannot reject a real positive overlap.
        heights = HeightInterval(
            nextafter(float(min(lower)), -float("inf")),
            nextafter(float(max(upper)), float("inf")),
        )
        object.__setattr__(self, "heights", heights)
        object.__setattr__(
            self,
            "_whole_overlap_m",
            (
                nextafter(float(max(lower)), float("inf")),
                nextafter(float(min(upper)), -float("inf")),
            ),
        )
        Reservation.__post_init__(self)

    def conflict_geometry(self, heights):
        if not isinstance(heights, HeightInterval):
            raise TypeError("explicit query height interval required")
        if not self.heights.overlaps(heights):
            return shapely.Polygon()
        max_lower, min_upper = self._whole_overlap_m
        if heights.upper_m > max_lower and heights.lower_m < min_upper:
            return self.geometry
        x0, y0, x1, y1 = self.geometry.bounds
        vertices = [
            tuple(map(rational, p)) for p in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))
        ]
        original = vertices
        for plane, limit, sign in (
            (self.lower_plane_m, heights.upper_m, -1),
            (self.upper_plane_m, heights.lower_m, 1),
        ):
            values = [
                sign * (plane_value(plane, self.origin_xy_m, p) - rational(limit))
                for p in vertices
            ]
            vertices = clip_chain(vertices, values)
            if not vertices:
                return shapely.Polygon()
        if vertices == original:
            return self.geometry
        # GEOS and the resulting XY vertices are binary64, not exact arithmetic.
        rectangle = shapely.Polygon([(float(x), float(y)) for x, y in vertices])
        if not rectangle.is_valid:
            from stair_occupancy_precision import certified_empty

            return certified_empty(self, heights)
        result = self.geometry.intersection(rectangle)
        if not result.is_valid:
            raise OccupancyError("affine intersection is numerically unresolved")
        result = polygonal_conflict(result)
        if result.is_empty:
            from stair_occupancy_precision import certified_empty

            return certified_empty(self, heights)
        return result
