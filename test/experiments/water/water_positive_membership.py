"""Positive triangle/source membership without constructing overlay polygons."""

from dataclasses import dataclass
from fractions import Fraction
from itertools import chain

import numpy as np
from shapely import get_num_coordinates
from water_orientation_predicates import centroid, orientations


@dataclass(frozen=True)
class SourceEdges:
    edges: np.ndarray


def prepare_source(source):
    if source.geom_type not in ("Polygon", "MultiPolygon") or source.is_empty:
        raise ValueError("valid nonempty original source polygon required")
    if not 3 <= get_num_coordinates(source) <= 8192:
        raise ValueError("bounded original source rings required")
    if not source.is_valid:
        raise ValueError("valid original source polygon required")
    rows = []
    for part in (source,) if source.geom_type == "Polygon" else source.geoms:
        for ring in chain((part.exterior,), part.interiors):
            xy = np.asarray(ring.coords)[:, :2]
            rows.append(np.stack((xy[:-1], xy[1:]), axis=1))
    edges = np.concatenate(rows)
    if not np.isfinite(edges).all() or np.any(abs(edges) > 1e8):
        raise ValueError("finite bounded original source XY required")
    edges.setflags(write=False)
    return SourceEdges(edges)


def _boundary_interior(triangle, edges, budget, exact):
    low, high = triangle.min(axis=0), triangle.max(axis=0)
    budget.charge("boundary_bbox_tests", len(edges))
    keep = np.all((edges.max(axis=1) >= low) & (edges.min(axis=1) <= high), axis=1)
    edges = edges[keep]
    if not len(edges):
        return False
    signs = orientations(
        triangle[:, None, None, :],
        np.roll(triangle, -1, axis=0)[:, None, None, :],
        edges[None, :, :, :],
        budget,
        exact=exact,
    )
    # A segment has no strict interior intersection if an entire endpoint pair
    # is on/outside one triangle half-plane. Its own line is the fourth SAT axis.
    edges = edges[~np.any(np.all(signs <= 0, axis=2), axis=0)]
    if not len(edges):
        return False
    signs = orientations(
        edges[:, None, 0, :],
        edges[:, None, 1, :],
        triangle[None, :, :],
        budget,
        exact=exact,
    )
    return bool(np.any((signs.min(axis=1) < 0) & (signs.max(axis=1) > 0)))


def _inside(triangle, edges, budget, exact):
    rational, point, bounds = centroid(triangle, budget)
    budget.charge("ray_edge_tests", len(edges))
    y = edges[:, :, 1]
    above = y > bounds[1][1]
    certain_below = y <= bounds[0][1]
    for i, j in zip(*np.nonzero(~(above | certain_below)), strict=True):
        budget.tick("exact_ray_heights")
        above[i, j] = Fraction(float(y[i, j])) > rational[1]
    crossing = edges[above[:, 0] != above[:, 1]]
    if not len(crossing):
        return False
    signs = orientations(
        crossing[:, 0],
        crossing[:, 1],
        point,
        budget,
        exact=exact,
        c_bounds=bounds,
        rational_c=rational,
    )
    upward = crossing[:, 1, 1] > crossing[:, 0, 1]
    return bool(np.count_nonzero(np.where(upward, signs > 0, signs < 0)) % 2)


def positive_overlap(triangle, source, budget, *, exact=False):
    """Return the exact positive-area predicate or refuse under the work budget.

    If a boundary segment meets the open triangle, source interior occupies a
    positive part of it. Otherwise membership is constant over the connected
    open triangle, so its rational centroid decides. Ring parity retains holes.
    Areas, representative-owner ranking and union coverage are separate outputs.
    """
    triangle = np.asarray(triangle, dtype=float)
    if (
        not isinstance(source, SourceEdges)
        or triangle.shape != (3, 2)
        or not np.isfinite(triangle).all()
        or np.any(abs(triangle) > 1e8)
        or type(exact) is not bool
    ):
        raise ValueError(
            "bounded original triangle, prepared source and exact-mode flag required"
        )
    sign = int(orientations(*triangle, budget, exact=exact))
    if not sign:
        raise ValueError("positive exact triangle area required")
    if sign < 0:
        triangle = triangle[[0, 2, 1]]
    return _boundary_interior(triangle, source.edges, budget, exact) or _inside(
        triangle, source.edges, budget, exact
    )
