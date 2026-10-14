"""Partition exterior roof UVs by the native charts incident to an original eave."""

from dataclasses import dataclass

import numpy as np
import shapely


@dataclass(frozen=True)
class ExteriorChart:
    polygon: object
    triangle: int
    station_range_m: tuple


def polygon_parts(geometry,limit):
    stack=[geometry];visited=0
    while stack:
        part=stack.pop();visited+=1
        if visited>4*limit+1:raise ValueError('Exterior UV collection capacity exceeded')
        if part.geom_type=='Polygon':
            if part.area>0:yield part
        elif part.geom_type in ('MultiPolygon','GeometryCollection'):
            stack.extend(reversed(list(part.geoms)))


def incident_charts(corners, a, b, tolerance=1e-8):
    length = np.linalg.norm(b-a)
    if length <= tolerance:raise ValueError('Exterior UV eave is unresolved')
    direction = (b-a)/length
    left = np.roll(corners,-1,axis=1)
    station = (corners-a)@direction
    other = (left-a)@direction
    normal = np.array([-direction[1],direction[0]])
    on_line = (abs((corners-a)@normal)<=tolerance)&(abs((left-a)@normal)<=tolerance)
    low, high = np.minimum(station,other), np.maximum(station,other)
    selected = on_line&(low>=-tolerance)&(high<=length+tolerance)&(high-low>tolerance)
    triangles,edges = np.nonzero(selected)
    intervals = sorted((float(low[t,e]),float(high[t,e]),int(t)) for t,e in zip(triangles,edges,strict=True))
    if not intervals or abs(intervals[0][0])>tolerance or abs(intervals[-1][1]-length)>tolerance:
        raise ValueError('Native triangle charts do not cover the eave')
    bounds = [0.]
    for first,last in zip(intervals,intervals[1:]):
        if abs(first[1]-last[0])>tolerance:raise ValueError('Native eave charts overlap or leave a gap')
        bounds.append((first[1]+last[0])/2.)
    bounds.append(float(length))
    return direction,tuple((i,bounds[n],bounds[n+1]) for n,(_,_,i) in enumerate(intervals))


def partition_exterior_charts(corners, a, b, exterior, max_regions=65536):
    corners=np.asarray(corners,dtype=float);a=np.asarray(a,dtype=float);b=np.asarray(b,dtype=float)
    if corners.ndim!=3 or corners.shape[1:]!=(3,2) or not np.isfinite(corners).all():
        raise ValueError('Exterior UV partition requires finite native XY triangles')
    if a.shape!=(2,) or b.shape!=(2,) or not np.isfinite([a,b]).all():
        raise ValueError('Exterior UV eave requires finite XY endpoints')
    if type(max_regions)is not int or max_regions<=0:raise ValueError('Invalid exterior UV chart capacity')
    if exterior.is_empty:return ()
    if not exterior.is_valid:raise ValueError('Exterior UV domain must be valid')
    direction, intervals=incident_charts(corners,a,b)
    basis=np.array([direction,[-direction[1],direction[0]]])
    coordinates=(shapely.get_coordinates(exterior)-a)@basis.T
    low,south=coordinates.min(axis=0);high,north=coordinates.max(axis=0)
    margin=max(1.,high-low,north-south)
    if len(intervals)>max_regions:raise ValueError('Exterior UV chart capacity exceeded')
    charts=[]
    for number,(triangle,start,stop) in enumerate(intervals):
        start=low-margin if number==0 else max(low-margin,start)
        stop=high+margin if number==len(intervals)-1 else min(high+margin,stop)
        if stop<=start:continue
        mask=shapely.Polygon(np.array([[start,south-margin],[stop,south-margin],
                                       [stop,north+margin],[start,north+margin]])@basis+a)
        clipped=exterior if len(intervals)==1 else exterior.intersection(mask)
        for polygon in polygon_parts(clipped,max_regions):
            charts.append(ExteriorChart(polygon,triangle,(start,stop)))
            if len(charts)>max_regions:raise ValueError('Exterior UV fragment capacity exceeded')
    return tuple(charts)


def extrapolate_chart(corners, properties, triangle, points):
    corners=np.asarray(corners,dtype=float);properties=np.asarray(properties,dtype=float);points=np.asarray(points,dtype=float)
    if corners.ndim!=3 or corners.shape[1:]!=(3,2) or properties.ndim!=3 or properties.shape[:2]!=corners.shape[:2]:
        raise ValueError('Exterior interpolation requires matching native triangles and corner properties')
    if isinstance(triangle,bool) or not isinstance(triangle,(int,np.integer)) or not 0<=triangle<len(corners):
        raise ValueError('Exterior interpolation has an invalid source triangle')
    xy=corners[triangle]
    if points.ndim!=2 or points.shape[1]!=2 or not all(np.isfinite(a).all() for a in (xy,points,properties[triangle])):
        raise ValueError('Exterior interpolation requires finite XY points and corner properties')
    try:pair=np.linalg.solve(np.stack((xy[1]-xy[0],xy[2]-xy[0]),axis=1),(points-xy[0]).T).T
    except np.linalg.LinAlgError as error:raise ValueError('Exterior interpolation triangle is degenerate') from error
    weights=np.c_[1.-pair.sum(axis=1),pair]
    return weights@properties[triangle]
