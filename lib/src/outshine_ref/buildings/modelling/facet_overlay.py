"""Conformal planar cells preserve the original native-triangle interpolation domains."""

from dataclasses import dataclass

import numpy as np
import shapely

from .skeleton_plan import SkeletonBudgetExceeded


@dataclass(frozen=True)
class FacetOverlay:
    corners: object
    source_triangles: object
    facets: object
    cells: int
    precision_m: float


def ownership(polygons, points):
    pairs = shapely.STRtree(polygons).query(points, predicate='within')
    counts = np.bincount(pairs[0], minlength=len(points))
    if np.any(counts != 1):
        raise ValueError('Each overlay cell needs exactly one native triangle and one roof facet')
    result = np.empty(len(points), dtype=np.int64)
    result[pairs[0]] = pairs[1]
    return result


def triangulate_cells(cells, source, facets, limit, precision_m=0.):
    if not len(cells) or len(cells) > limit:
        raise SkeletonBudgetExceeded('Roof overlay cell capacity exceeded or empty domain')
    cells = np.asarray(cells, dtype=object)
    points = shapely.point_on_surface(cells)
    sources, owners = ownership(source, points), ownership(facets, points)
    triangles, indices = shapely.get_parts(shapely.constrained_delaunay_triangles(cells), return_index=True)
    if len(triangles) > 4*limit:
        raise SkeletonBudgetExceeded('Roof overlay triangle capacity exceeded')
    corners = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3].copy()
    first, last = corners[:, 1]-corners[:, 0], corners[:, 2]-corners[:, 0]
    reverse = first[:, 0]*last[:, 1]-first[:, 1]*last[:, 0] < 0
    corners[reverse] = corners[reverse][:, [0, 2, 1]]
    return FacetOverlay(corners, sources[indices], owners[indices], len(cells), precision_m)


def domains(source_corners, plan, limit):
    if type(limit) is not int or limit <= 0:
        raise ValueError('Roof overlay requires a positive cell/candidate capacity')
    if len(source_corners)*len(plan.facets) > limit:
        raise SkeletonBudgetExceeded('Conservative native-triangle/facet candidate capacity exceeded')
    source = shapely.polygons(source_corners)
    facets = np.asarray([facet.polygon for facet in plan.facets], dtype=object)
    tolerance = max(1e-7, plan.footprint.area*1e-10)
    union = shapely.union_all(source)
    if (union.symmetric_difference(plan.footprint).area > tolerance
            or shapely.area(source).sum()-union.area > tolerance):
        raise ValueError('Native triangles must cover the logical face without overlaps or courtyard fill')
    precision = max(1e-9, max(1., float(abs(source_corners).max()))*64*np.finfo(float).eps,
                    2*plan.maximum_refinement_m)
    source = shapely.set_precision(source, precision)
    facets = shapely.set_precision(facets, precision)
    if np.any(shapely.is_empty(source)) or np.any(shapely.is_empty(facets)):
        raise ValueError('Roof overlay cannot resolve the input at its floating-point precision')
    return source, facets, precision


def overlay(source_corners, plan, limit=65536, domain=None):
    if domain is not None and domain.is_empty:
        return FacetOverlay(np.empty((0,3,2)),np.empty(0,dtype=np.int64),np.empty(0,dtype=np.int64),0,0.)
    source, facets, precision = domains(source_corners, plan, limit)
    boundaries = np.r_[shapely.boundary(source), shapely.boundary(facets)]
    if domain is not None:boundaries=np.r_[boundaries,shapely.set_precision(domain,precision).boundary]
    network = shapely.union_all(boundaries, grid_size=precision)
    cells = shapely.get_parts(shapely.polygonize(shapely.get_parts(network)))
    retained = plan.footprint if domain is None else shapely.set_precision(domain, precision)
    cells = cells[shapely.covers(retained, shapely.point_on_surface(cells))]
    return triangulate_cells(cells, source, facets, limit, precision)
