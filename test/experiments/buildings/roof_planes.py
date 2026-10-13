"""Metric roof facets from a bounded straight skeleton or analytic planar profiles."""

import math
import threading
import time as clock

import numpy as np
import shapely
from shapely.affinity import affine_transform
import py_straight_skeleton.algorithm as skeleton_algorithm

from building_layout import AREA_TOL_M2, frame, polygons


MAX_VERTICES = 256
MAX_EVENTS = 4096
SKELETON_SECONDS = 2.
TRACER_LOCK = threading.Lock()


class Budget(skeleton_algorithm.NoopAlgorithmTracer):
    def __init__(self, events):
        self.events = events
        self.began = clock.perf_counter()
        self.steps = 0

    def on_step_begin(self, step, time, algorithm):
        self.steps = step
        if step > self.events:
            raise RuntimeError('roof skeleton exceeds its event budget')
        if clock.perf_counter()-self.began > SKELETON_SECONDS:
            raise RuntimeError('roof skeleton exceeds its two-second worker budget')


def plane(polygon, points, heights, origin):
    points = np.asarray(points)
    matrix = np.c_[points-origin,np.ones(len(points))]
    coefficients,_,rank,_ = np.linalg.lstsq(matrix,np.asarray(heights),rcond=None)
    if rank != 3 or np.max(abs(matrix@coefficients-heights)) > 1e-6:
        raise ValueError('roof facet is degenerate or non-planar')
    if not polygon.is_valid or polygon.area <= 1e-8:
        raise ValueError('roof facet is not a usable polygon')
    return dict(polygon=polygon,plane=coefficients)


def height(facet, points, origin):
    points = np.asarray(points)
    return (points-origin)@np.asarray(facet['plane'][:2])+facet['plane'][2]


def skeleton(footprint, events=MAX_EVENTS):
    if sum(len(ring.coords)-1 for ring in [footprint.exterior,*footprint.interiors]) > MAX_VERTICES:
        raise ValueError('roof footprint exceeds the 256-vertex budget')
    origin = np.array(footprint.centroid.coords[0])
    shape = shapely.orient_polygons(footprint)
    exterior = (np.array(shape.exterior.coords)[:-1]-origin).tolist()
    holes = [(np.array(ring.coords)[:-1]-origin).tolist() for ring in shape.interiors]
    with TRACER_LOCK:
        previous = skeleton_algorithm.GLOBAL_ALGORITHM_TRACER
        budget = Budget(events)
        skeleton_algorithm.set_global_algorithm_tracer(budget)
        try:
            result = skeleton_algorithm.StraightSkeletonAlgorithm().compute_skeleton(exterior,holes)
        finally:
            skeleton_algorithm.set_global_algorithm_tracer(previous)
    if len(result.nodes)>4*MAX_VERTICES:
        raise ValueError('roof skeleton exceeds its node budget')
    points = np.array([[node.position.x,node.position.y] for node in result.nodes])+origin
    times = np.array([node.time for node in result.nodes])
    if not np.isfinite(times).all() or times.min() < 0 or times.max() <= 1e-8:
        raise ValueError('roof skeleton has no finite positive rise')
    facets = [plane(shapely.Polygon(points[indices]),points[indices],times[indices]/times.max(),origin)
              for indices in result.get_faces()]
    return dict(facets=facets,origin=origin,method='straight-skeleton',events=budget.steps,
                skeleton_time_m=float(times.max()))


def rectangle(footprint):
    outer = footprint.minimum_rotated_rectangle
    if footprint.symmetric_difference(outer).area>AREA_TOL_M2:
        raise ValueError('analytic hip roof requires a rectangular source footprint')
    forward,reverse = frame(outer)
    local = affine_transform(outer,forward)
    a,b,c,d = local.bounds
    radius = (d-b)/2
    if radius<=1e-8 or c-a<d-b-1e-7:
        raise ValueError('analytic rectangle has invalid metric axes')
    corners = np.array([[a,b],[c,b],[c,d],[a,d]])
    ridge = np.array([[a+radius,(b+d)/2],[c-radius,(b+d)/2]])
    records = [([0,1],[1,0]),([1,2],[1]),([2,3],[0,1]),([3,0],[0])]
    origin = np.array(footprint.centroid.coords[0])
    facets = []
    for edge,peaks in records:
        points = np.r_[corners[edge],ridge[peaks]]
        polygon = affine_transform(shapely.Polygon(points),reverse)
        world = np.array(polygon.exterior.coords)[:-1]
        z = np.r_[np.zeros(len(edge)),np.ones(len(peaks))]
        facets.append(plane(polygon,world,z,origin))
    return dict(facets=facets,origin=origin,method='analytic-rectangle',events=0,skeleton_time_m=radius)


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
            result = rectangle(footprint) if footprint.symmetric_difference(footprint.minimum_rotated_rectangle).area<=AREA_TOL_M2 else skeleton(footprint)
        else:
            raise ValueError('unsupported hip-roof construction method')
    else:
        result = profile(footprint,kind,direction,across)
    return check(result|dict(kind=kind),footprint)
