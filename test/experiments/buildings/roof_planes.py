"""Metric roof facets from a bounded straight skeleton or analytic planar profiles."""

import math

import numpy as np
import shapely
from shapely.affinity import affine_transform
from outshine_ref.buildings.modelling import SkeletonLimits, prepare_hip

from building_layout import AREA_TOL_M2, frame, polygons


MAX_VERTICES = 256
MAX_EVENTS = 4096
SKELETON_SECONDS = 2.


def height(facet, points, origin):
    points = np.asarray(points)
    return (points-origin)@np.asarray(facet['plane'][:2])+facet['plane'][2]


def prepared(footprint, method, events=MAX_EVENTS):
    limits = SkeletonLimits(vertices=MAX_VERTICES, python_events=events, seconds=SKELETON_SECONDS)
    plan = prepare_hip(footprint, method, limits)
    rise = float(plan.distances_m.max())
    facets = [dict(polygon=shapely.affinity.translate(facet.polygon, *plan.origin),
                   plane=np.r_[facet.gradient, facet.intercept_m]/rise) for facet in plan.facets]
    return dict(facets=facets, origin=np.asarray(plan.origin),
                method='analytic-rectangle' if plan.method=='rectangle' else 'straight-skeleton',
                events=plan.events, skeleton_time_m=rise, backend=plan.method, fallback_reason=plan.fallback_reason)


def skeleton(footprint, events=MAX_EVENTS):
    return prepared(footprint, 'skeleton', events)


def rectangle(footprint):
    return prepared(footprint, 'rectangle')


def profile(footprint, kind, direction=None, across=False):
    origin = np.array(footprint.centroid.coords[0])
    forward,_ = frame(footprint)
    axis = np.array(forward[2:4] if kind=='gable' else forward[:2])
    if across:
        axis = np.array([-axis[1],axis[0]])
    if direction is not None:
        angle = math.radians(direction)
        axis = np.array([math.sin(angle),math.cos(angle)])
        if kind=='shed':
            axis = -axis
    corners = np.concatenate([np.array(ring.coords) for ring in [footprint.exterior,*footprint.interiors]])
    values = (corners-origin)@axis
    lo,hi = float(values.min()),float(values.max())
    span = hi-lo
    if span<=1e-8:
        raise ValueError('roof profile has no usable run')
    if kind=='flat':
        return dict(facets=[dict(polygon=footprint,plane=np.array([0.,0.,0.]))],origin=origin,method='flat',events=0)
    planes = [(np.r_[axis/span,-lo/span],footprint)]
    if kind=='gable':
        tangent = np.array([axis[1],-axis[0]])
        normal_extent = np.max(abs((corners-origin)@tangent))+1.
        centre = (hi+lo)/2
        halves = []
        for low,high in ((lo-1.,centre),(centre,hi+1.)):
            box = shapely.Polygon([origin+tangent*x+axis*y for x,y in
                                   ((-normal_extent,low),(normal_extent,low),(normal_extent,high),(-normal_extent,high))])
            halves.append(footprint.intersection(box))
        planes = [(np.r_[axis/(span/2),-lo/(span/2)],halves[0]),
                  (np.r_[-axis/(span/2),hi/(span/2)],halves[1])]
    facets = [dict(polygon=part,plane=coefficients) for coefficients,geometry in planes for part in polygons(geometry)]
    return dict(facets=facets,origin=origin,method='single-ridge-profile' if kind=='gable' else 'single-plane-profile',events=0)


def check(design, footprint):
    facets = design['facets']
    union = shapely.union_all([facet['polygon'] for facet in facets])
    overlap = sum(facet['polygon'].area for facet in facets)-union.area
    if union.symmetric_difference(footprint).area>AREA_TOL_M2 or overlap>AREA_TOL_M2:
        raise ValueError('roof facets leave a gap, overlap or fill a source courtyard')
    for facet in facets:
        points = np.array(facet['polygon'].exterior.coords)
        values = height(facet,points,design['origin'])
        if values.min() < -1e-7 or values.max() > 1.+1e-7:
            raise ValueError('roof planes leave the normalized source-height interval')
    return design


def build(footprint, kind='hip', method='auto', direction=None, across=False):
    if sum(len(ring.coords)-1 for ring in [footprint.exterior,*footprint.interiors]) > MAX_VERTICES:
        raise ValueError('roof footprint exceeds the 256-vertex budget')
    if kind not in ('flat','hip','gable','shed'):
        raise ValueError('unsupported roof shape')
    if kind=='hip':
        if method=='straight-skeleton':
            result = skeleton(footprint)
        elif method=='analytic-rectangle':
            result = rectangle(footprint)
        elif method=='auto':
            result = prepared(footprint, 'auto')
        else:
            raise ValueError('unsupported hip-roof construction method')
    else:
        result = profile(footprint,kind,direction,across)
    return check(result|dict(kind=kind),footprint)
