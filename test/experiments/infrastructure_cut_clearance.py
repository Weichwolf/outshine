"""Check directed vehicle poses before committing shared surface cuts."""

import numpy as np
import shapely

from infrastructure_flat_corners import rounded_plan
from infrastructure_port_lanes import project_lane_positions
from infrastructure_travel import MODES
from infrastructure_vehicle_sweep import body_outlines


def motor_model(graphs,complete,vehicle,radius,error):
    lines,half,flags = [],[],[]
    for graph in graphs:
        if 'travel_public' not in graph:
            continue
        permitted = graph['travel_public']&MODES['motor']!=0
        keep = np.any(permitted,axis=1)
        lines.extend(graph['lines'][keep]);half.extend(graph['half_widths'][keep]);flags.extend(permitted[keep])
    lines,half = np.array(lines,dtype=object),np.array(half)
    flags = np.array(flags,dtype=bool).reshape(-1,2)
    bands = shapely.buffer(lines,half,quad_segs=2,cap_style='round',join_style='round')
    surface = shapely.union_all(bands)
    if radius>0:
        surface,_ = rounded_plan(surface,radius,error)
    surface = shapely.intersection(surface,complete)
    shapely.prepare(surface)
    keep = half>=vehicle['widthM']*.5+error
    lines,half,flags = lines[keep],half[keep],flags[keep]
    return dict(lines=lines,half=half,flags=flags,index=shapely.STRtree(lines),surface=surface)


def invalid_vehicle_sections(spans,motor,vehicle,error,traffic_side=1):
    if not len(spans) or not len(motor['lines']):
        return np.empty(0,dtype=int),0
    lines,half,flags = motor['lines'],motor['half'],motor['flags']
    cuts = shapely.linestrings(spans)
    cut,axis = motor['index'].query(cuts,predicate='intersects')
    parts,owners = shapely.get_parts(shapely.intersection(cuts[cut],lines[axis]),return_index=True)
    types = shapely.get_type_id(parts)
    bad = cut[owners[types!=0]].tolist()
    cut,axis = cut[owners[types==0]],axis[owners[types==0]]
    points = shapely.get_coordinates(parts[types==0])
    station = shapely.line_locate_point(lines[axis],parts[types==0])
    length = shapely.length(lines[axis])
    before = shapely.get_coordinates(shapely.line_interpolate_point(lines[axis],np.maximum(0,station-.01)))
    after = shapely.get_coordinates(shapely.line_interpolate_point(lines[axis],np.minimum(length,station+.01)))
    tangent = after-before
    tangent /= np.linalg.norm(tangent,axis=1)[:,None]
    rows,columns = np.nonzero(flags[axis])
    cut,axis = cut[rows],axis[rows]
    heading = tangent[rows]*np.where(columns==0,1.,-1.)[:,None]
    split = np.all(flags[axis],axis=1)&(half[axis]*.5>=vehicle['widthM']*.5+error)
    offset = np.where(split,half[axis]*.5*traffic_side,0.)
    position,valid = project_lane_positions(points[rows],heading,offset,spans[cut,0],spans[cut,1])
    bad.extend(cut[~valid].tolist())
    cut,position,heading = cut[valid],position[valid],heading[valid]
    valid = shapely.distance(shapely.points(position),cuts[cut])<=1e-6
    bodies = shapely.polygons(body_outlines(position,np.arctan2(heading[:,1],heading[:,0]),
                               vehicle['widthM']*.5+error,vehicle['frontFromRearAxleM']+error,
                               vehicle['rearOverhangM']+error))
    valid &= shapely.covers(motor['surface'],bodies)
    bad.extend(cut[~valid].tolist())
    return np.unique(bad).astype(int),len(cut)
