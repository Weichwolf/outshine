"""Whole-domain numerical comparison of two continuous piecewise-affine heights.

The maximum on each convex triangle intersection occurs at an intersection vertex.
The P1 squared-error integral is evaluated exactly algebraically on a triangle fan.
Caller meshes use one common fixed ENU frame. GEOS/binary64 arithmetic is used;
this is not an interval or Earth-source bound.
"""

import math
from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.contracts import Mesh


@dataclass(frozen=True)
class FieldDifference:
    report: dict
    overlay_xy: np.ndarray
    overlay_error_m: np.ndarray


def surface(mesh):
    if (
        not isinstance(mesh, Mesh)
        or not 0 < len(mesh.triangles) <= 16384
        or len(mesh.vertices) > 8192
    ):
        raise ValueError("bounded nonempty mesh required")
    points = mesh.vertices[mesh.triangles]
    edges = points[:, 1:, :2] - points[:, :1, :2]
    determinant = edges[:, 0, 0] * edges[:, 1, 1] - edges[:, 0, 1] * edges[:, 1, 0]
    if np.any(determinant == 0) or not np.isfinite(determinant).all():
        raise ValueError("height is not a nondegenerate XY graph")
    gradient = np.linalg.solve(edges, (points[:, 1:, 2] - points[:, :1, 2])[..., None])[
        ..., 0
    ]
    polygons = shapely.polygons(points[:, :, :2])
    area = float(shapely.area(polygons).sum())
    union = shapely.union_all(polygons)
    if abs(area - union.area) > max(1e-8, area * 1e-12):
        raise ValueError("input height triangles overlap")
    return points, gradient, polygons, area


def height(points, gradient, index, xy):
    return points[index, 0, 2] + (xy - points[index, 0, :2]) @ gradient[index]


def compare_fields(first, second, *, pair_limit=262144):
    if type(pair_limit) is not int or not 0 < pair_limit <= 262144:
        raise ValueError("bounded positive triangle-pair budget required")
    a, ga, pa, area_a = surface(first)
    b, gb, pb, area_b = surface(second)
    tree = shapely.STRtree(pb)
    moments, overlay_xy, overlay_error = [], [], []
    maximum, worst = -1.0, None
    work = {
        "tree_queries": 0,
        "max_query_hits": 0,
        "candidate_pairs": 0,
        "positive_area_pairs": 0,
        "overlay_vertices": 0,
        "fan_triangles": 0,
    }
    for i, polygon in enumerate(pa):
        hits = tree.query(polygon)
        work["tree_queries"] += 1
        work["max_query_hits"] = max(work["max_query_hits"], len(hits))
        for j in sorted(map(int, hits)):
            if work["candidate_pairs"] == pair_limit:
                raise ValueError("field comparison pair budget exhausted")
            work["candidate_pairs"] += 1
            cut = polygon.intersection(pb[j])
            if cut.area <= 0:
                continue
            if cut.geom_type != "Polygon" or len(cut.interiors):
                raise ValueError(
                    "positive triangle intersection is not a convex polygon"
                )
            xy = np.asarray(cut.exterior.coords)[:-1]
            error = height(a, ga, i, xy) - height(b, gb, j, xy)
            at = int(np.argmax(abs(error)))
            if abs(error[at]) > maximum:
                maximum = float(abs(error[at]))
                worst = {
                    "first_face": i,
                    "second_face": j,
                    "xy_m": xy[at].tolist(),
                    "signed_enu_up_difference_m": float(error[at]),
                }
            work["positive_area_pairs"] += 1
            work["overlay_vertices"] += len(xy)
            for k in range(1, len(xy) - 1):
                indices = [0, k, k + 1]
                u, v = xy[indices[1:]] - xy[0]
                area = abs(u[0] * v[1] - u[1] * v[0]) / 2
                e = error[indices]
                square = (
                    area / 6 * (np.sum(e * e) + e[0] * e[1] + e[0] * e[2] + e[1] * e[2])
                )
                moments.append((float(area), float(area * e.mean()), float(square)))
                overlay_xy.append(xy[indices])
                overlay_error.append(e)
                work["fan_triangles"] += 1
    overlap = math.fsum(x[0] for x in moments)
    tolerance = max(1e-8, max(area_a, area_b) * 1e-12)
    if overlap <= 0 or max(abs(area_a - overlap), abs(area_b - overlap)) > tolerance:
        raise ValueError("fields do not cover the same complete XY domain")
    report = {
        "scope": "all affine overlap facets in binary64/GEOS; no sample-only or Earth-source accuracy claim",
        "maximum_enu_up_difference_m": maximum,
        "integrated_enu_up_rms_m": math.sqrt(
            max(0.0, math.fsum(x[2] for x in moments) / overlap)
        ),
        "mean_enu_up_difference_m": math.fsum(x[1] for x in moments) / overlap,
        "first_area_m2": area_a,
        "second_area_m2": area_b,
        "overlay_area_m2": overlap,
        "worst": worst,
        "work": work,
    }
    return FieldDifference(report, np.asarray(overlay_xy), np.asarray(overlay_error))
