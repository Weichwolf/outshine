"""Restrict vehicle turns to public motor surfaces and the actual shared mesh."""

import numpy as np
import shapely

from transport_flat_corners import rounded_plan
from transport_travel import MODES
from transport_surface_regions import polygonal_region


def mesh_surface_index(product):
    triangles = shapely.polygons(product['vertices'][product['indices']])
    return triangles,shapely.STRtree(triangles)


def public_pedestrian_surface(shape,graph,tree):
    ids=tree.query(shape,predicate='intersects')
    public,restricted=graph['travel_public'][ids],graph['travel_restricted'][ids]
    walk=np.any(public&MODES['walk']!=0,axis=1)&~np.any(public&(MODES['motor']|MODES['rail'])!=0,axis=1)&~np.any(restricted!=0,axis=1)
    ids=ids[walk]
    return polygonal_region(shapely.union_all(shapely.buffer(graph['lines'][ids],graph['half_widths'][ids],quad_segs=2))),ids


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
    return polygonal_region(shapely.intersection(area,physical))


def same_island_winding(path,route,surface):
    rings = [ring for part in shapely.get_parts(surface) for ring in part.interiors]
    if not rings:
        return True
    markers = shapely.get_coordinates(shapely.point_on_surface(shapely.polygons(rings)))
    loop = np.vstack((shapely.get_coordinates(route),shapely.get_coordinates(path)[::-1]))
    lower,upper = loop.min(axis=0),loop.max(axis=0)
    markers = markers[np.all((markers>=lower)&(markers<=upper),axis=1)]
    for point in markers:
        a = loop-point
        b = np.roll(a,-1,axis=0)
        cross = a[:,0]*b[:,1]-a[:,1]*b[:,0]
        positive = (a[:,1]<=0)&(b[:,1]>0)&(cross>0)
        negative = (a[:,1]>0)&(b[:,1]<=0)&(cross<0)
        if np.count_nonzero(positive)!=np.count_nonzero(negative):
            return False
    return True
