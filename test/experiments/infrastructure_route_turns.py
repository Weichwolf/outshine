"""Follow directed source routes around islands using bounded G2 corner recipes."""

import numpy as np
import shapely

from infrastructure_junction_turns import connect_lanes
from infrastructure_vehicle_sweep import straight_body_check


def rounded_route(points,surface,vehicle,error,maximum_corners=64):
    steps = np.diff(points,axis=0)
    lengths = np.linalg.norm(steps,axis=1)
    if np.any(lengths<=1e-7):
        return None,dict(reason='collapsed_route_segment')
    directions = steps/lengths[:,None]
    changes = np.arctan2(directions[:-1,0]*directions[1:,1]-directions[:-1,1]*directions[1:,0],
                        np.einsum('ij,ij->i',directions[:-1],directions[1:]))
    corners = np.flatnonzero(np.abs(changes)>1e-7)+1
    if len(corners)>maximum_corners:
        return None,dict(reason='route_corner_budget_exhausted')
    position,heading = points[0],directions[0]
    parts,notes = [],[]
    for index in corners:
        trim = .45*min(lengths[index-1],lengths[index])
        entry = dict(position=points[index]-trim*directions[index-1],heading=directions[index-1])
        exit = dict(position=points[index]+trim*directions[index],heading=directions[index])
        line,note = connect_lanes(entry,exit,surface,vehicle,error)
        if line is None:
            return None,dict(reason='unresolved_route_corner',corner=int(index),detail=note)
        check,_ = straight_body_check(surface,position,entry['position'],np.arctan2(heading[1],heading[0]),vehicle,error)
        if not check['fits_surface']:
            return None,dict(reason='route_straight_body_outside_surface')
        parts.extend((np.array([position,entry['position']]),shapely.get_coordinates(line)))
        notes.extend((check,note))
        position,heading = exit['position'],exit['heading']
    check,_ = straight_body_check(surface,position,points[-1],np.arctan2(heading[1],heading[0]),vehicle,error)
    if not check['fits_surface']:
        return None,dict(reason='route_straight_body_outside_surface')
    notes.append(check)
    parts.append(np.array([position,points[-1]]))
    xy = np.vstack(parts)
    xy = xy[np.r_[True,np.linalg.norm(np.diff(xy,axis=0),axis=1)>1e-10]]
    path = shapely.LineString(xy)
    if not shapely.is_simple(path):
        return None,dict(reason='self_intersecting_rounded_route')
    radii = [n['minimum_radius_m'] for n in notes if n.get('minimum_radius_m') is not None]
    samples = sum(n.get('vehicle_sweep',n)['sampled_poses'] for n in notes)
    return path,dict(profile='G2-route',corners=len(corners),length_m=path.length,
                     minimum_radius_m=min(radii) if radii else None,endpoint_curvature_per_m=0.,
                     vehicle_sweep=dict(fits_surface=True,sampled_poses=samples),pieces=notes)


def source_route_connector(entry,exit,route,surface,vehicle,error=.025):
    raw = shapely.get_coordinates(route)
    if max(np.linalg.norm(raw[0]-entry['position']),np.linalg.norm(raw[-1]-exit['position']))>1e-6:
        return None,dict(reason='requires_offset_source_route')
    if len(raw)<3:
        return None,dict(reason='source_route_has_no_intermediate_corner')
    for tolerance in (error,error*2,error*4,error*8,error*16):
        interior = shapely.LineString(raw[1:-1]).simplify(tolerance) if len(raw)>3 else shapely.Point(raw[1])
        points = np.vstack((raw[0],shapely.get_coordinates(interior),raw[-1]))
        directions = points[[1,-1]]-points[[0,-2]]
        directions /= np.linalg.norm(directions,axis=1)[:,None]
        if min(directions[0]@entry['heading'],directions[1]@exit['heading'])<1-1e-8:
            return None,dict(reason='source_route_port_heading_mismatch')
        path,note = rounded_route(points,surface,vehicle,error)
        if path is not None:
            return path,dict(simplification_m=tolerance,**note)
    return None,note
