"""Extract mode-permitted lane poses from the actual shared module cross-sections."""

import numpy as np
import shapely

from infrastructure_travel import MODES
from infrastructure_flat_ports import shared_port_sections


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


def junction_shape(product,index,face_index=None):
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


def crossing_lane_poses(graph,edge,crossing,port,shape,flags,vehicle,traffic_side,error):
    tangent,positive = crossing_tangent(graph['lines'][edge],crossing,shape)
    if tangent is None:
        return [],['ambiguous_port_interior_direction']
    half,body_half = graph['half_widths'][edge],vehicle['widthM']*.5+error
    if half<body_half:
        return [],['insufficient_mode_width']
    point = np.array(crossing.coords[0])
    ends = shapely.get_coordinates(port['line'])
    port_direction = ends[-1]-ends[0]
    normal = np.array([-port_direction[1],port_direction[0]])
    shared_lane = bool(np.all(flags) and half*.5<body_half)
    poses,unused = [],[]
    for column in np.flatnonzero(flags):
        heading = tangent*(1 if column==0 else -1)
        right = np.array([heading[1],-heading[0]])
        offset = half*.5*traffic_side if np.all(flags) and not shared_lane else 0.
        shifted = point+right*offset
        denominator = heading@normal
        if abs(denominator)<=1e-10*np.linalg.norm(normal):
            unused.append('lane_tangent_parallel_to_port')
            continue
        shifted -= heading*((shifted-ends[0])@normal/denominator)
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
