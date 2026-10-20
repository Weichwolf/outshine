"""Classify prepared Shell enclosures against a supplied regional occluder projection."""

import itertools

import numpy as np
import shapely
from prepared_source_support import affine_ranges, world_corners
from region_occlusion import outer_ranges, project_ranges


def select_sources(product, requested, projection, *, lod="shell"):
    if product["header"]["identity"] != projection[
        "support_identity"
    ] or not np.array_equal(
        product["header"]["anchor_ecef"], projection["anchor_ecef"]
    ):
        raise ValueError(
            "prepared support and projection reference different sources/anchors"
        )
    basis = np.asarray(projection["basis"], float)
    if (
        basis.shape != (3, 3)
        or not np.all((abs(basis) == 0) | (abs(basis) == 1))
        or not np.all(abs(basis).sum(0) == 1)
        or not np.all(abs(basis).sum(1) == 1)
    ):
        raise ValueError("signed axis projection frame required")
    domain = np.asarray(projection["observer_box"], float)
    if (
        domain.shape != (2, 3)
        or not np.isfinite(domain).all()
        or np.any(domain[0] > domain[1])
    ):
        raise ValueError("finite ordered observer domain required")
    eyes = np.unique(
        np.array(list(itertools.product(*zip(*domain, strict=True)))) @ basis.T, axis=0
    )
    requested = np.asarray(requested)
    if requested.ndim != 1 or not np.issubdtype(requested.dtype, np.integer):
        raise ValueError("one-dimensional source ordinals required")
    hidden = np.zeros(len(requested), bool)
    available = np.zeros_like(hidden)
    work = {
        "source_queries": len(requested),
        "available_sources": 0,
        "depth_candidates": 0,
        "projected_interval_corners": 0,
        "query_polygon_vertices": 0,
    }
    frame = np.vstack((np.zeros(3), basis.T))
    plane, guard, fused = (
        projection["plane"],
        projection["guard_projected_m"],
        projection["fused"],
    )
    for row, ordinal in enumerate(requested):
        points = world_corners(product, ordinal, lod=lod)
        if points is None:
            continue
        available[row] = True
        points = affine_ranges(points, frame)
        if points[0][:, 2].min() <= plane + guard:
            continue
        work["depth_candidates"] += 1
        projected = project_ranges(points, eyes, plane)
        query = outer_ranges(projected, guard)
        work["projected_interval_corners"] += len(points[0]) * len(eyes)
        work["query_polygon_vertices"] += len(shapely.get_coordinates(query))
        hidden[row] = fused.covers(query) and fused.boundary.distance(query) > guard
    work["available_sources"] = int(available.sum())
    return hidden, available, work
