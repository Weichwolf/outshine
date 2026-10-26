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


def overlap_parts(first, second, mask):
    cut = first.intersection(second)
    if cut.area <= 0:
        return
    if mask is None:
        if cut.geom_type != "Polygon" or len(cut.interiors):
            raise ValueError("positive triangle intersection is not a convex polygon")
        yield cut
        return
    cut = cut.intersection(mask)
    if shapely.get_num_coordinates(cut) > 4096:
        raise ValueError("masked overlay coordinate budget exhausted")
    for part in shapely.get_parts(cut):
        if part.geom_type != "Polygon" or part.area <= 0:
            continue
        yield from shapely.get_parts(shapely.constrained_delaunay_triangles(part))


def compare_fields(
    first, second, *, pair_limit=262144, domain_mask=None, fan_limit=262144
):
    """Compare equal full domains, or both complete fields on an explicit mask.

    A mask changes the declared comparison domain, never the physical constraints
    or input field. Both owners must cover its whole area, including all islands
    and disconnected water components. Root planes, rather than ill-conditioned
    clipped child planes, evaluate the differences at masked overlay vertices.
    """
    if type(pair_limit) is not int or not 0 < pair_limit <= 262144:
        raise ValueError("bounded positive triangle-pair budget required")
    a, ga, pa, area_a = surface(first)
    b, gb, pb, area_b = surface(second)
    if type(fan_limit) is not int or not 0 < fan_limit <= 262144:
        raise ValueError("bounded positive overlay-fan budget required")
    missing_a = missing_b = 0.0
    if domain_mask is not None:
        if (
            not isinstance(domain_mask, shapely.Geometry)
            or domain_mask.geom_type not in ("Polygon", "MultiPolygon")
            or domain_mask.is_empty
            or not domain_mask.is_valid
            or not np.isfinite(shapely.get_coordinates(domain_mask)).all()
            or shapely.get_num_coordinates(domain_mask) > 4096
        ):
            raise ValueError("bounded valid polygonal comparison mask required")
        tolerance = max(1e-8, domain_mask.area * 1e-12)
        missing_a = domain_mask.difference(shapely.union_all(pa)).area
        missing_b = domain_mask.difference(shapely.union_all(pb)).area
        if max(missing_a, missing_b) > tolerance:
            raise ValueError("fields do not cover the complete declared mask")
    tree = shapely.STRtree(pb)
    moments, overlay_xy, overlay_error = [], [], []
    maximum, worst = -1.0, None
    work = {
        "tree_queries": 0,
        "max_query_hits": 0,
        "candidate_pairs": 0,
        "positive_area_pairs": 0,
        "positive_overlay_parts": 0,
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
            positive_pair = False
            for cut in overlap_parts(polygon, pb[j], domain_mask):
                if cut.area <= 0:
                    continue
                if not positive_pair:
                    work["positive_area_pairs"] += 1
                    positive_pair = True
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
                work["positive_overlay_parts"] += 1
                work["overlay_vertices"] += len(xy)
                for k in range(1, len(xy) - 1):
                    if work["fan_triangles"] == fan_limit:
                        raise ValueError(
                            "field comparison overlay-fan budget exhausted"
                        )
                    indices = [0, k, k + 1]
                    u, v = xy[indices[1:]] - xy[0]
                    area = abs(u[0] * v[1] - u[1] * v[0]) / 2
                    e = error[indices]
                    square = (
                        area
                        / 6
                        * (np.sum(e * e) + e[0] * e[1] + e[0] * e[2] + e[1] * e[2])
                    )
                    moments.append((float(area), float(area * e.mean()), float(square)))
                    overlay_xy.append(xy[indices])
                    overlay_error.append(e)
                    work["fan_triangles"] += 1
    overlap = math.fsum(x[0] for x in moments)
    expected = (area_a, area_b) if domain_mask is None else (domain_mask.area,)
    tolerance = max(1e-8, max(expected) * 1e-12)
    if overlap <= 0 or max(abs(area - overlap) for area in expected) > tolerance:
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
        "comparison_domain": "equal_mesh_domains"
        if domain_mask is None
        else "explicit_water_mask",
        "first_mask_missing_area_m2": missing_a,
        "second_mask_missing_area_m2": missing_b,
        "worst": worst,
        "work": work,
    }
    return FieldDifference(report, np.asarray(overlay_xy), np.asarray(overlay_error))
