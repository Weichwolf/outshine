"""Clip immutable hip height fields without changing their retained ridges."""

from dataclasses import dataclass

import numpy as np
import shapely
from shapely import affinity

from .hip_surface import SurfacePatch
from .scope import scalar


@dataclass(frozen=True, slots=True)
class HipInset:
    field: object
    facets: tuple
    domain: object
    eave_level_m: float
    precision_m: float

    @property
    def footprint(self):return self.field.footprint

    @property
    def maximum_refinement_m(self):return self.field.maximum_refinement_m


def half_plane(polygon, gradient, intercept, level):
    gradient = np.asarray(gradient, dtype=float)
    length = np.linalg.norm(gradient)
    if length == 0:
        return polygon if intercept >= level else shapely.Polygon()
    x, y = gradient/length
    projected = affinity.affine_transform(polygon, [x, y, -y, x, 0., 0.])
    low, south, high, north = projected.bounds
    cut = (level-intercept)/length
    if cut <= low:return polygon
    if cut >= high:return shapely.Polygon()
    result = projected.intersection(shapely.box(cut, south, high, north))
    return affinity.affine_transform(result, [x, -y, y, x, 0., 0.])


def prepare_hip_inset(field, value, mode, overhang):
    overhang = scalar(overhang)
    if overhang >= 0:raise ValueError('Hip inset requires a negative overhang')
    factor = field.factor(value, mode)
    distance = -overhang/np.hypot(1., factor)
    precision = max(1e-9, max(1., max(abs(x) for x in field.footprint.bounds))*64*np.finfo(float).eps,
                    2*field.maximum_refinement_m)
    facets = []
    for number, facet in enumerate(field.facets):
        clipped = shapely.set_precision(half_plane(facet.polygon, facet.gradient, facet.intercept_m, distance), precision)
        for polygon in shapely.get_parts(clipped):
            if polygon.geom_type != 'Polygon' or polygon.is_empty or polygon.area <= 0:continue
            if not polygon.is_valid:raise ValueError('Invalid clipped hip surface')
            facets.append(SurfacePatch(polygon, facet.gradient, facet.intercept_m,
                                       getattr(facet, 'facet', number), facet.node_indices))
    domain = shapely.union_all([f.polygon for f in facets], grid_size=precision)
    if not domain.is_valid:raise ValueError('Invalid hip inset domain')
    return HipInset(field, tuple(facets), domain, float(distance*factor), precision)
