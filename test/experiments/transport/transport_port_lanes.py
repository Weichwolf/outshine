"""Extract mode-permitted lane poses from the actual shared module cross-sections."""

import numpy as np
import shapely

from transport_travel import MODES
from transport_flat_ports import shared_port_sections
from transport_vehicle_sweep import body_outlines


def junction_interfaces(product):
    sections = shared_port_sections(product)
    if not np.all(sections['valid']):
        raise ValueError('lane ports require straight connected shared cross-sections')
    result = {}
    for index,(pair,begin,end) in enumerate(zip(sections['parts'],sections['begin'],sections['end'])):
        core = product['part_roles'][pair].astype(bool)
        junction,corridor = int(pair[core][0]),int(pair[~core][0])
        result.setdefault(junction,[]).append(dict(corridor=corridor,section=index,
                                                  line=shapely.LineString([begin,end])))
    return result


def module_face_index(product):
    owners = product['face_owners']
    return np.argsort(owners,kind='stable'),np.r_[0,np.cumsum(np.bincount(owners,minlength=len(product['part_roles'])))]


def module_shape(product,index,face_index=None):
    rows = product['face_owners']==index if face_index is None else face_index[0][face_index[1][index]:face_index[1][index+1]]
    triangles = product['vertices'][product['indices'][rows]]
    return shapely.union_all(shapely.polygons(triangles))


def crossing_tangent(axis,crossing,shape):
    station = axis.project(crossing)
    span = min(.01,axis.length*.1)
    a,b = axis.interpolate(max(0,station-span)),axis.interpolate(min(axis.length,station+span))
    tangent = np.array(b.coords[0])-np.array(a.coords[0])
    length = np.linalg.norm(tangent)
    if length<=1e-10:
        return None,None
    for step in span*.25**np.arange(10):
        sample = [axis.interpolate(min(axis.length,station+step)),axis.interpolate(max(0,station-step))]
        positive,negative = shapely.contains(shape,sample)
        if positive!=negative:
            return tangent/length,bool(positive)
    return None,None


def project_lane_positions(points,headings,offset,begin,end):
    direction = end-begin
    normal = np.stack((-direction[...,1],direction[...,0]),axis=-1)
    denominator = np.sum(headings*normal,axis=-1)
    valid = np.abs(denominator)>1e-10*np.linalg.norm(normal,axis=-1)
    right = np.stack((headings[...,1],-headings[...,0]),axis=-1)
    position = points+right*np.asarray(offset)[...,None]
    along = np.divide(np.sum((position-begin)*normal,axis=-1),denominator,
                      out=np.zeros_like(denominator),where=valid)
    return position-headings*along[...,None],valid


def crossing_lane_poses(graph,edge,crossing,port,shape,flags,vehicle,traffic_side,error):
    tangent,positive = crossing_tangent(graph['lines'][edge],crossing,shape)
    if tangent is None:
        return [],['ambiguous_port_interior_direction']
    half,body_half = graph['half_widths'][edge],vehicle['widthM']*.5+error
    if half<body_half:
        return [],['insufficient_mode_width']
    point = np.array(crossing.coords[0])
    ends = shapely.get_coordinates(port['line'])
    shared_lane = bool(np.all(flags) and half*.5<body_half)
    poses,unused = [],[]
    for column in np.flatnonzero(flags):
        heading = tangent*(1 if column==0 else -1)
        offset = half*.5*traffic_side if np.all(flags) and not shared_lane else 0.
        shifted,valid = project_lane_positions(point,heading,offset,ends[0],ends[-1])
        if not valid:
            unused.append('lane_tangent_parallel_to_port')
            continue
        if port['line'].distance(shapely.Point(shifted))>1e-6:
            unused.append('lane_outside_port')
            continue
        poses.append(dict(position=shifted,heading=heading,incoming=positive if column==0 else not positive,
                          edge=int(edge),direction=1 if column==0 else -1,
                          station=float(graph['lines'][edge].project(crossing)),
                          corridor=port['corridor'],section=port['section'],shared_lane=shared_lane))
    return poses,unused


def lane_poses(graph,ports,shape,vehicle,mode='motor',traffic_side=1,error=.025,tree=None):
    if traffic_side not in (-1,1):
        raise ValueError('traffic side must be right or left')
    tree = shapely.STRtree(graph['lines']) if tree is None else tree
    bit = MODES[mode]
    poses,unused = [],[]
    for port in ports:
        for edge in tree.query(port['line'],predicate='intersects'):
            flags = graph['travel_public'][edge]&bit!=0
            if not np.any(flags):
                continue
            crossing = shapely.intersection(graph['lines'][edge],port['line'])
            if crossing.geom_type not in ('Point','MultiPoint'):
                unused.append(dict(edge=int(edge),section=port['section'],reason='ambiguous_port_axis_crossing'))
                continue
            for point in shapely.get_parts(crossing):
                local,reasons = crossing_lane_poses(graph,edge,point,port,shape,flags,vehicle,traffic_side,error)
                poses.extend(local)
                unused.extend(dict(edge=int(edge),section=port['section'],reason=r) for r in reasons)
    return poses,unused


def align_vehicle_ports(poses,surface,vehicle,error=.025):
    if not poses:
        return [],[]
    positions = np.array([p['position'] for p in poses])
    angles = np.array([np.arctan2(p['heading'][1],p['heading'][0]) for p in poses])
    def bodies(points,headings):
        return shapely.polygons(body_outlines(points,headings,vehicle['widthM']*.5,
                                vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']))
    shapely.prepare(surface)
    invalid = np.flatnonzero(~shapely.covers(surface,bodies(positions,angles)))
    result,adjusted = [dict(p) for p in poses],[]
    boundary = shapely.boundary(surface)
    degrees = np.arange(-30,31,dtype=float)
    for index in invalid:
        headings = angles[index]+np.deg2rad(degrees)
        rectangles = bodies(np.repeat(positions[index,None,:],len(headings),axis=0),headings)
        clearance = shapely.distance(rectangles,boundary)
        valid = shapely.covers(surface,rectangles)&(clearance>=error)
        choices = np.flatnonzero(valid)
        if not len(choices):
            continue
        chosen = choices[np.argmax(clearance[choices])]
        angle = headings[chosen]
        result[index]['heading'] = np.array([np.cos(angle),np.sin(angle)])
        adjusted.append(dict(port=int(index),rotation_deg=float(degrees[chosen])))
    return result,adjusted
