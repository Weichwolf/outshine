"""Restrict vehicle turns to public motor surfaces and the actual shared mesh."""

import numpy as np
import shapely

from infrastructure_flat_corners import rounded_plan
from infrastructure_travel import MODES


def mesh_surface_index(product):
    triangles = shapely.polygons(product['vertices'][product['indices']])
    return triangles,shapely.STRtree(triangles)


def motor_surface(shape,graph,tree,mesh,vehicle,corner_radius=2.,error=.025):
    padding = max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])+float(graph['half_widths'].max())+corner_radius
    a,b,c,d = shape.bounds
    bounds = shapely.box(a-padding,b-padding,c+padding,d+padding)
    source_bounds = shapely.buffer(bounds,float(graph['half_widths'].max())+corner_radius,join_style='mitre')
    edges = tree.query(source_bounds,predicate='intersects')
    edges = edges[np.any(graph['travel_public'][edges]&MODES['motor']!=0,axis=1)]
    if not len(edges):
        return shapely.Polygon()
    axes = shapely.intersection(graph['lines'][edges],source_bounds)
    bands = shapely.buffer(axes,graph['half_widths'][edges],quad_segs=2,cap_style='round',join_style='round')
    area,_ = rounded_plan(shapely.union_all(bands),corner_radius,error)
    triangles,index = mesh
    physical = shapely.union_all(shapely.intersection(triangles[index.query(bounds,predicate='intersects')],bounds))
    return shapely.intersection(area,physical)
