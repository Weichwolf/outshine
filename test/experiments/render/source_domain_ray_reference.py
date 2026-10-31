"""Bounded front-facing ray intersections with complete planar source domains.

This evaluates unsnapped analytic planes, not native welded triangles, material
derivatives, raster sampling or World admission. Uncertain callers keep originals.
"""

import numpy as np
import shapely


def intersect(patches, origins, directions, *, pairs_most=4194304):
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
    if (
        origins.ndim != 2
        or origins.shape[1] != 3
        or directions.shape != origins.shape
        or not np.isfinite(origins).all()
        or not np.isfinite(directions).all()
        or np.any(np.linalg.norm(directions, axis=1) == 0)
        or not 1 <= len(patches) <= 65536
        or len(origins) * len(patches) > pairs_most
    ):
        raise ValueError("Bounded finite rays and complete domain inventory required")
    depth = np.full(len(origins), np.inf)
    owner = np.full(len(origins), -1, dtype=np.int32)
    plane_tests = inside_tests = 0
    for i, patch in enumerate(patches):
        surface = patch.geometry
        axes, normal, origin = (
            np.asarray(v, float) for v in (surface.axes, surface.normal, surface.origin)
        )
        if (
            axes.shape != (2, 3)
            or normal.shape != (3,)
            or origin.shape != (3,)
            or not np.isfinite(axes).all()
            or not np.isfinite(normal).all()
            or not np.isfinite(origin).all()
            or not surface.domain.is_valid
        ):
            raise ValueError("Valid finite source patch required")
        gram = axes @ axes.T
        if np.linalg.det(gram) <= 0:
            raise ValueError("Independent domain axes required")
        denominator = directions @ normal
        numerator = (origin - origins) @ normal
        distance = np.divide(
            numerator,
            denominator,
            out=np.full(len(origins), np.inf),
            where=denominator < -1e-12,
        )
        candidate = np.flatnonzero(
            (denominator < -1e-12) & (distance > 0.1) & (distance < depth)
        )
        plane_tests += len(origins)
        if not len(candidate):
            continue
        points = (
            origins[candidate]
            + distance[candidate, None] * directions[candidate]
            - origin
        )
        local = points @ axes.T @ np.linalg.inv(gram)
        covered = shapely.covers(surface.domain, shapely.points(local))
        inside_tests += len(candidate)
        hit = candidate[covered]
        depth[hit], owner[hit] = distance[hit], i
    return (
        depth,
        owner,
        {"plane_tests": plane_tests, "domain_point_tests": inside_tests},
    )
