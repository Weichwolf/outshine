"""Bounded affine deck candidates from whole lower-facet intersections."""

import math

import numpy as np
import shapely
from scipy.optimize import linprog


def lower_constraints(lower, domain, required, *, points_most=65536):
    """Use every clipped affine-facet corner, including holes and interior peaks."""
    if (
        not 0 < len(lower.triangles) <= 32768
        or not math.isfinite(required)
        or required < 0
        or type(points_most) is not int
        or not 1 <= points_most <= 65536
        or domain.is_empty
        or not domain.is_valid
    ):
        raise ValueError("Bounded lower surface/domain and clearance required")
    xyz = lower.vertices[lower.triangles].astype(float)
    shapes = shapely.polygons(xyz[:, :, :2])
    hits = shapely.STRtree(shapes).query(domain, predicate="intersects")
    if not len(hits):
        return np.empty((0, 3)), {"candidate_faces": 0, "constraint_points": 0}
    clipped = shapely.intersection(shapes[hits], domain)
    positive = shapely.area(clipped) > 0
    clipped, xyz = clipped[positive], xyz[hits[positive]]
    count = int(shapely.get_num_coordinates(clipped).sum())
    if count > points_most:
        raise ValueError("Whole lower-overlap point budget before expansion")
    if not count:
        return np.empty((0, 3)), {
            "candidate_faces": len(hits),
            "constraint_points": 0,
        }
    xy, owner = shapely.get_coordinates(clipped, return_index=True)
    edges = xyz[:, 1:, :2] - xyz[:, :1, :2]
    gradient = np.linalg.solve(edges, (xyz[:, 1:, 2] - xyz[:, :1, 2])[..., None])[
        ..., 0
    ]
    z = xyz[owner, 0, 2] + np.einsum(
        "ij,ij->i", xy - xyz[owner, 0, :2], gradient[owner]
    )
    points = np.column_stack((xy, z + required))
    if not np.isfinite(points).all():
        raise ValueError("Finite affine overlap constraints required")
    return points, {
        "candidate_faces": len(hits),
        "positive_overlap_faces": len(clipped),
        "constraint_points": len(points),
        "constraint_array_bytes": points.nbytes,
    }


def slope_rows(axes, along, cross):
    if (
        not isinstance(axes, np.ndarray)
        or axes.ndim != 2
        or axes.shape[1] != 2
        or not 1 <= len(axes) <= 2048
        or not np.isfinite(axes).all()
        or not np.isfinite([along, cross]).all()
        or min(along, cross) <= 0
    ):
        raise ValueError(
            "Bounded finite rail axes and positive profile limits required"
        )
    lengths = np.linalg.norm(axes, axis=1)
    if np.any(lengths < 1e-6):
        raise ValueError("Nondegenerate whole rail segments required")
    tangent = axes / lengths[:, None]
    normal = np.column_stack((-tangent[:, 1], tangent[:, 0]))
    directions = np.vstack((tangent, -tangent, normal, -normal))
    limits = np.r_[np.full(2 * len(axes), along), np.full(2 * len(axes), cross)]
    return directions, limits


def fit_plane(points, domain, axes, *, along=0.04, cross=0.03, method="tilted"):
    """Minimize area-mean deck height within this affine family and fixed bounds."""
    if (
        not isinstance(points, np.ndarray)
        or points.ndim != 2
        or points.shape[1] != 3
        or not 1 <= len(points) <= 65536
        or not np.isfinite(points).all()
        or domain.is_empty
        or not domain.is_valid
        or domain.area <= 0
        or method not in ("flat", "tilted")
    ):
        raise ValueError("Bounded finite whole-footprint constraints/method required")
    directions, limits = slope_rows(axes, along, cross)
    origin = np.asarray(domain.centroid.coords[0])
    relative = points[:, :2] - origin
    if abs(relative).max() > 8190:
        raise ValueError("Bounded local affine deck frame required")
    design = np.column_stack((relative, np.ones(len(points))))
    if method == "flat":
        coefficients = np.array([0.0, 0.0, points[:, 2].max()])
        status, iterations = "direct maximum, no solver", 0
    else:
        result = linprog(
            [0, 0, 1],
            A_ub=np.vstack(
                (-design, np.column_stack((directions, np.zeros(len(limits)))))
            ),
            b_ub=np.r_[-points[:, 2], limits],
            bounds=[(None, None)] * 3,
            method="highs-ds",
            options={"time_limit": 2.0, "maxiter": 10000},
        )
        if not result.success or result.x is None:
            raise ValueError("Affine deck LP refused: " + result.message)
        coefficients, status, iterations = result.x, result.message, result.nit
    residual = design @ coefficients - points[:, 2]
    if (
        not np.isfinite(coefficients).all()
        or residual.min() < -1e-7
        or np.any(directions @ coefficients[:2] - limits > 1e-10)
    ):
        raise ValueError("Actual affine deck failed whole height/profile constraints")
    return (
        origin,
        coefficients,
        {
            "method": method,
            "origin_xy_m": origin.tolist(),
            "coefficients": coefficients.tolist(),
            "area_mean_height_m": float(coefficients[2]),
            "minimum_constraint_residual_m": float(residual.min()),
            "lower_points": len(points),
            "profile_inequalities": len(limits),
            "lp_variables": 0 if method == "flat" else 3,
            "lp_rows": 0 if method == "flat" else len(points) + len(limits),
            "design_bytes": design.nbytes,
            "iterations": int(iterations),
            "status": status,
            "scope": "This fixed affine-family height objective only, no globally optimal road, C1/curvature/structural/routing/World/device authority. Clearance checked independently on actual Float64/Float32 meshes.",
        },
    )
