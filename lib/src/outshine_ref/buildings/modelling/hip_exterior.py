"""Continuous positive roof collars in intrinsic metric face coordinates."""

from dataclasses import dataclass

import numpy as np
import shapely

@dataclass(frozen=True)
class ExteriorPatch:
    polygon: object
    gradient: tuple
    intercept_m: float
    owner: int


@dataclass(frozen=True)
class ExteriorSurface:
    patches: tuple
    domain: object
    base_level_m: float
    precision_m: float
    candidate_pairs: int
    collapsed_triangles: tuple


def prepare_hip_exterior(plan,value=3.,mode='byHeight',overhang=.5,max_pairs=65536,shell_depth_m=.1):
    """Continuous CDT collar with a bounded miter and a fixed outer eave height."""
    if not np.isfinite(shell_depth_m) or shell_depth_m<=0:raise ValueError('Exterior shell depth must be positive')
    if not np.isfinite(overhang) or overhang<=0:raise ValueError('Collar needs positive overhang')
    if type(max_pairs)is not int or max_pairs<=0:raise ValueError('Invalid collar capacity')
    factor=plan.factor(value,mode);distance=overhang/np.hypot(1.,factor);drop=distance*factor
    precision=max(1e-9,max(1.,max(abs(x) for x in plan.footprint.bounds))*64*np.finfo(float).eps,2*plan.maximum_refinement_m)
    original=shapely.set_precision(plan.footprint,precision)
    domain=shapely.set_precision(shapely.buffer(original,distance,join_style='mitre',mitre_limit=5.),precision)
    annulus=shapely.difference(domain,original,grid_size=precision)
    triangles=shapely.get_parts(shapely.constrained_delaunay_triangles(annulus))
    if len(triangles)*3>max_pairs:raise ValueError('Collar triangle capacity exceeded')
    rings=(plan.footprint.exterior,*plan.footprint.interiors)
    edges=[]
    for facet in plan.facets:
        ring,edge=facet.boundary;edges.append(shapely.LineString(np.asarray(rings[ring].coords)[edge:edge+2]))
    edges=np.asarray(edges,dtype=object);patches=[]
    for triangle in triangles:
        corners=np.asarray(triangle.exterior.coords)[:3]
        inner=shapely.distance(shapely.points(corners),original.boundary)<=2*precision
        outer=shapely.distance(shapely.points(corners),domain.boundary)<=2*precision
        if not np.all(inner|outer):raise ValueError('CDT collar created a non-boundary vertex')
        heights=np.where(inner,0.,-drop)
        center=corners.mean(axis=0);center_height=heights.mean()
        for edge in range(3):
            stop=(edge+1)%3;points=np.array([corners[edge],corners[stop],center])
            levels=np.array([heights[edge],heights[stop],center_height])
            matrix=np.c_[points-points[0],np.ones(3)]
            gradient=np.linalg.solve(matrix,levels)[:2]
            intercept=float(levels[0]-points[0]@gradient)
            polygon=shapely.Polygon(points)
            segment=shapely.LineString(points[:2])
            incident=shapely.intersection(edges,segment)
            overlap=shapely.length(incident)
            if overlap.max()>segment.length-2*precision:owner=int(np.argmax(overlap))
            else:owner=int(np.argmin(shapely.distance(edges,polygon.centroid)))
            patches.append(ExteriorPatch(polygon,tuple(gradient),intercept,owner))
    return ExteriorSurface(tuple(patches),domain,-float(drop)-float(shell_depth_m),precision,len(triangles)*3,())
