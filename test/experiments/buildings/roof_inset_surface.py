"""Alternative negative overhang: truncate the fixed roof field at its eave level."""

from dataclasses import dataclass

import numpy as np
import shapely
from shapely import affinity

from outshine_ref.buildings.modelling.hip_surface import prepare_even


@dataclass(frozen=True)
class InsetPatch:
    polygon: object
    gradient: tuple
    intercept_m: float
    owner: int
    precision_m: float


def half_plane(polygon, gradient, intercept, level):
    gradient = np.asarray(gradient, dtype=float)
    length = np.linalg.norm(gradient)
    if length == 0:
        return polygon if intercept >= level else shapely.Polygon()
    x, y = gradient/length
    projected = affinity.affine_transform(polygon, [x, y, -y, x, 0., 0.])
    low, south, high, north = projected.bounds
    cut = (level-intercept)/length
    if cut <= low:
        return polygon
    if cut >= high:
        return shapely.Polygon()
    result = projected.intersection(shapely.box(cut, south, high, north))
    return affinity.affine_transform(result, [x, -y, y, x, 0., 0.])


def inset(plan, value, mode, overhang, even=False):
    if not np.isfinite(overhang) or overhang >= 0:
        raise ValueError('Inset surface requires a finite negative overhang')
    field = prepare_even(plan, mode) if even else plan
    factor = field.factor(value, mode)
    level = -overhang*factor/np.hypot(1., factor)
    precision = max(1e-9, max(1., max(abs(x) for x in plan.footprint.bounds))*64*np.finfo(float).eps,
                    2*plan.maximum_refinement_m)
    patches = []
    for number, facet in enumerate(field.facets):
        gradient = np.asarray(facet.gradient)*factor
        intercept = facet.intercept_m*factor
        clipped = shapely.set_precision(half_plane(facet.polygon, gradient, intercept, level), precision)
        for polygon in shapely.get_parts(clipped):
            if polygon.geom_type == 'Polygon' and not polygon.is_empty and polygon.area > 0:
                if not polygon.is_valid:
                    raise ValueError('Invalid clipped roof surface')
                owner = facet.facet if even else number
                patches.append(InsetPatch(polygon, tuple(gradient), float(intercept), owner, precision))
    return tuple(patches), float(level)
