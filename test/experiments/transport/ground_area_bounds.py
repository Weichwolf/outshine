"""Independent rational whole-cap/terrain bounds on actual stored geometry."""

import math
from collections import Counter
from fractions import Fraction as F

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_occupancy_geometry import clip_chain
from surface_solid_checks import audit_closed


def exact_points(triangle):
    return tuple(tuple(F(float(v)) for v in point) for point in triangle)


def area(polygon):
    return (
        sum(
            a[0] * b[1] - a[1] * b[0]
            for a, b in zip(polygon, polygon[1:] + polygon[:1], strict=True)
        )
        / 2
        if polygon
        else F(0)
    )


def plane(triangle):
    a, b, c = exact_points(triangle)
    u = tuple(x - y for x, y in zip(b, a, strict=True))
    v = tuple(x - y for x, y in zip(c, a, strict=True))
    determinant = u[0] * v[1] - u[1] * v[0]
    if determinant <= 0:
        raise ValueError("Actual upward stored height triangle required")
    gx = (u[2] * v[1] - u[1] * v[2]) / determinant
    gy = (u[0] * v[2] - u[2] * v[0]) / determinant
    return a[2] - gx * a[0] - gy * a[1], gx, gy


def value(coefficients, xy):
    return coefficients[0] + coefficients[1] * xy[0] + coefficients[2] * xy[1]


def intersection(first, second):
    polygon = list(first)
    for a, b in zip(second, second[1:] + second[:1], strict=True):
        if not polygon:
            break
        values = [
            (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])
            for p in polygon
        ]
        polygon = clip_chain(polygon, values)
    return polygon


def rounded(value, upper):
    result = float(value)
    if (upper and F(result) < value) or (not upper and F(result) > value):
        result = math.nextafter(result, math.inf if upper else -math.inf)
    return result


def cap_bounds(cap, terrain, *, pairs_most=32768):
    if len(cap.triangles) > 8192 or len(terrain.triangles) > 512:
        raise ValueError("Bounded stored paving and terrain geometry required")
    ground = terrain.vertices[terrain.triangles]
    boxes = np.array(
        [shapely.box(*t[:, :2].min(0), *t[:, :2].max(0)) for t in ground], object
    )
    tree = shapely.STRtree(boxes)
    terrain_planes = [plane(t) for t in ground]
    minimum, maximum, grade2 = None, None, F(0)
    pairs = vertices = 0
    for triangle in cap.vertices[cap.triangles]:
        coefficient = plane(triangle)
        grade2 = max(grade2, coefficient[1] ** 2 + coefficient[2] ** 2)
        polygon = tuple(p[:2] for p in exact_points(triangle))
        wanted = area(polygon)
        covered = F(0)
        hits = tree.query(shapely.box(*triangle[:, :2].min(0), *triangle[:, :2].max(0)))
        pairs += len(hits)
        if pairs > pairs_most:
            raise ValueError("Exact whole-cap pair budget exhausted")
        for index in hits:
            cell = intersection(
                polygon, tuple(p[:2] for p in exact_points(ground[index]))
            )
            if not cell or area(cell) <= 0:
                continue
            covered += area(cell)
            vertices += len(cell)
            differences = [
                value(coefficient, p) - value(terrain_planes[index], p) for p in cell
            ]
            a, b = min(differences), max(differences)
            minimum = a if minimum is None else min(minimum, a)
            maximum = b if maximum is None else max(maximum, b)
        if covered != wanted:
            raise ValueError("Actual entire cap lacks exact held terrain coverage")
    if minimum is None:
        raise ValueError("Nonempty whole source cap required")
    return (
        minimum,
        maximum,
        grade2,
        {"candidate_triangle_pairs": pairs, "exact_intersection_vertices": vertices},
    )


def body_caps(body, upper, lower):
    def signature(triangle):
        values = tuple(map(int, triangle))
        return min(values, values[1:] + values[:1], values[2:] + values[:2])

    indices = {tuple(v): i for i, v in enumerate(body.vertices)}
    top = np.array([indices[tuple(v)] for v in upper.vertices])
    bottom = np.array([indices[tuple(v)] for v in lower.vertices])
    expected = Counter(signature(f) for f in top[upper.triangles])
    expected.update(signature(f[::-1]) for f in bottom[lower.triangles])
    top_set, bottom_set = set(top), set(bottom)
    actual, edge_count, walls = Counter(), Counter(), Counter()
    for face in upper.triangles:
        for a, b in zip(face, np.roll(face, -1), strict=True):
            edge_count[
                frozenset((tuple(upper.vertices[a, :2]), tuple(upper.vertices[b, :2])))
            ] += 1
    boundary = {e for e, n in edge_count.items() if n == 1}
    for face in body.triangles:
        if set(face) <= top_set or set(face) <= bottom_set:
            actual[signature(face)] += 1
        else:
            edge = frozenset(tuple(v) for v in body.vertices[face, :2])
            if len(edge) != 2 or edge not in boundary:
                raise ValueError(
                    "Mixed body triangles must be actual cap boundary walls"
                )
            walls[edge] += 1
    if (
        actual != expected
        or set(walls) != boundary
        or any(n != 2 for n in walls.values())
    ):
        raise ValueError(
            "Actual body must have its whole declared caps and boundary walls"
        )


def verify_geometry(body, upper, lower, terrain, original, recipe):
    if not np.array_equal(
        upper.vertices[:, :2], lower.vertices[:, :2]
    ) or not np.array_equal(upper.triangles, lower.triangles):
        raise ValueError("Whole ordered caps require shared complete XY topology")
    if len(body.vertices) != 2 * len(upper.vertices) or {
        tuple(v) for v in body.vertices
    } != {tuple(v) for v in np.vstack((upper.vertices, lower.vertices))}:
        raise ValueError("Actual whole body must contain exactly its declared caps")
    body_caps(body, upper, lower)
    footprint = shapely.coverage_union_all(
        shapely.polygons(upper.vertices[upper.triangles, :2])
    )
    if (
        not shapely.coverage_is_valid(
            shapely.polygons(upper.vertices[upper.triangles, :2])
        )
        or footprint.geom_type != "Polygon"
        or len(footprint.interiors) != len(original.interiors)
        or not footprint.covers(original)
        or not original.buffer(6 * 2**-11, quad_segs=16).covers(footprint)
    ):
        raise ValueError(
            "Complete original footprint/holes and physical model bound required"
        )
    reports = []
    for dtype in (np.float64, np.float32):
        stored = Mesh(body.vertices.astype(dtype).astype(float), body.triangles)
        top = Mesh(upper.vertices.astype(dtype).astype(float), upper.triangles)
        bottom = Mesh(lower.vertices.astype(dtype).astype(float), lower.triangles)
        old = Mesh(terrain.vertices.astype(dtype).astype(float), terrain.triangles)
        volume = audit_closed(stored)
        if volume <= 0 or not np.array_equal(
            top.vertices[:, :2], upper.vertices[:, :2]
        ):
            raise ValueError("Stored floor topology or exact XY footprint changed")
        a, b, grade, top_work = cap_bounds(top, old)
        c, d, _bottom_grade, bottom_work = cap_bounds(bottom, old)
        if (
            a < F(recipe.minimum_rise_m)
            or b > F(recipe.maximum_rise_m)
            or d > -F(recipe.minimum_embed_m)
            or grade > F(recipe.maximum_grade) ** 2
        ):
            raise ValueError(
                "Actual whole stored paving violates rise/embed/grade recipe"
            )
        reports.append(
            {
                "storage": np.dtype(dtype).name,
                "minimum_rise_m": rounded(a, False),
                "maximum_rise_m": rounded(b, True),
                "minimum_embed_m": rounded(-d, False),
                "maximum_embed_m": rounded(-c, True),
                "grade_squared_upper": rounded(grade, True),
                "positive_closed_volume_m3": volume,
                "top_work": top_work,
                "bottom_work": bottom_work,
            }
        )
    return {
        "whole_supported_floor": True,
        "stored_caps": reports,
        "scope": "Exact rational continuous cap/held-terrain intersection extrema on supplied Double and stored Float32 geometry. No GPU arithmetic, route, accessibility, Source/World or device admission.",
    }
