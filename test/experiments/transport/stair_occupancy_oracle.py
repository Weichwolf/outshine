"""Optional SFCGAL oracle with independent rational half-plane vertex enumeration."""

from fractions import Fraction as F
from itertools import combinations, pairwise


def constraints(row, heights):
    x0, y0, x1, y1 = map(F, row.geometry.bounds)
    ox, oy = map(F, row.origin_xy_m)
    low, high = (tuple(map(F, p)) for p in (row.lower_plane_m, row.upper_plane_m))
    return [
        (F(1), F(0), -x0),
        (-F(1), F(0), x1),
        (F(0), F(1), -y0),
        (F(0), -F(1), y1),
        (-low[1], -low[2], F(heights.upper_m) - low[0] + low[1] * ox + low[2] * oy),
        (high[1], high[2], high[0] - F(heights.lower_m) - high[1] * ox - high[2] * oy),
    ]


def cross(a, b, c):
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def window(row, heights):
    """Enumerate feasible line-pair intersections, rather than clipping a chain."""
    planes = constraints(row, heights)
    if any(a == b == 0 and c <= 0 for a, b, c in planes[4:]):
        return ()
    points = set()
    for (a, b, c), (d, e, f) in combinations(planes, 2):
        determinant = a * e - b * d
        if determinant:
            x, y = (b * f - e * c) / determinant, (c * d - a * f) / determinant
            if all(gx * x + gy * y + z >= 0 for gx, gy, z in planes):
                points.add((x, y))
    if len(points) < 3:
        return ()
    points = sorted(points)
    chains = []
    for order in (points, points[::-1]):
        chain = []
        for p in order:
            while len(chain) > 1 and cross(chain[-2], chain[-1], p) <= 0:
                chain.pop()
            chain.append(p)
        chains.extend(chain[:-1])
    return tuple(chains) if len(chains) >= 3 else ()


def native_ring_area(text):
    if len(text) > 1024**2 or not text.startswith(("LINESTRING (", "TRIANGLE ((")):
        raise ValueError("bounded rational native ring export required")
    coordinates = text.rpartition("(")[2].partition(")")[0]
    points = [tuple(map(F, p.split())) for p in coordinates.split(",")]
    if not points or any(len(p) != 2 for p in points) or points[0] != points[-1]:
        raise ValueError("complete closed two-dimensional native ring required")
    return abs(sum(a[0] * b[1] - b[0] * a[1] for a, b in pairwise(points))) / 2


def native_area(geometry):
    """Read rational coordinates before any binary64 geometry export."""
    if geometry.is_empty or geometry.dimension < 2:
        return F(0)
    if geometry.has_z or geometry.has_m:
        raise ValueError("expected a two-dimensional oracle result")
    if geometry.geom_type in (
        "MultiPolygon",
        "GeometryCollection",
        "TriangulatedSurface",
        "PolyhedralSurface",
    ):
        return sum(native_area(part) for part in geometry)
    if geometry.geom_type == "Triangle":
        return native_ring_area(geometry.to_wkt())
    if geometry.geom_type != "Polygon":
        raise ValueError("expected an exact polygonal oracle result")
    return sum(
        (1 if i == 0 else -1) * native_ring_area(r.to_wkt())
        for i, r in enumerate(geometry)
    )


def native_source(shape):
    import shapely
    from pysfcgal.geometry import Geometry

    source = Geometry.from_wkb(shapely.orient_polygons(shape).wkb)
    if source is None or not source.is_valid():
        raise ValueError("native oracle source conversion/validity failed")
    return source


def oracle_geometry(row, heights, *, domain=None):
    from pysfcgal.geometry import Geometry

    vertices = window(row, heights)
    if not vertices:
        return None
    wkt = "POLYGON((" + ",".join(f"{x} {y}" for x, y in (*vertices, vertices[0])) + "))"
    exact_window = Geometry.from_wkt(wkt)
    source = native_source(row.geometry if domain is None else domain)
    if (
        exact_window is None
        or source is None
        or not exact_window.is_valid()
        or not source.is_valid()
    ):
        raise ValueError("native oracle input conversion/validity failed")
    result = source.intersection(exact_window)
    if result is None or not result.is_valid():
        raise ValueError("native oracle intersection/validity failed")
    return result


def oracle(row, heights):
    result = oracle_geometry(row, heights)
    return F(0) if result is None else native_area(result)
