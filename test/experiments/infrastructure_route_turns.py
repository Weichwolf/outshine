"""Follow directed source routes around islands using bounded G2 corner recipes."""

import numpy as np
import shapely

from infrastructure_junction_turns import clothoid_connector,curvature_connector
from infrastructure_vehicle_sweep import body_outlines
from infrastructure_spline_routes import spline_route_candidates


def route_waypoint_poses(points,maximum_corners=64):
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
    positions,headings = [points[0]],[directions[0]]
    for index in corners:
        trim = .45*min(lengths[index-1],lengths[index])
        bisector = directions[index-1]+directions[index]
        norm = np.linalg.norm(bisector)
        if norm<=1e-7:
            return None,dict(reason='source_route_requires_return_turn')
        positions.extend((points[index]-trim*directions[index-1],points[index],points[index]+trim*directions[index]))
        headings.extend((directions[index-1],bisector/norm,directions[index]))
    positions.append(points[-1]);headings.append(directions[-1])
    return (np.array(positions),np.array(headings)),None


def rounded_route(points,surface,vehicle,error,lookahead=8,maximum_corners=64,connectors=None,ports=None):
    connectors = {} if connectors is None else connectors
    poses,reason = route_waypoint_poses(points,maximum_corners)
    if reason is not None:
        return None,reason
    positions,headings = poses
    if ports is not None:
        positions[[0,-1]] = [p['position'] for p in ports]
        headings[[0,-1]] = [p['heading'] for p in ports]
    bodies = body_outlines(positions,np.arctan2(headings[:,1],headings[:,0]),vehicle['widthM']*.5,
                           vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    shapely.prepare(surface)
    possible = shapely.covers(surface,shapely.polygons(bodies))
    if not possible[0] or not possible[-1]:
        return None,dict(reason='source_route_port_body_outside_surface')
    scores,parents,candidates = {0:(0,0.)},{},0
    for i in range(len(positions)-1):
        if i not in scores:
            continue
        entry = dict(position=positions[i],heading=headings[i])
        for j in range(i+1,min(i+lookahead+1,len(positions))):
            if not possible[j]:
                continue
            lower = scores[i][0]+1,scores[i][1]+np.linalg.norm(positions[j]-positions[i])
            if lower>=scores.get(j,(np.inf,np.inf)):
                continue
            candidates += 1
            exit = dict(position=positions[j],heading=headings[j])
            key = positions[[i,j]].tobytes(),headings[[i,j]].tobytes()
            if key not in connectors:
                path,note = curvature_connector(entry,exit,surface,vehicle,error)
                if path is None:
                    path,note = clothoid_connector(entry,exit,surface,vehicle,error)
                connectors[key] = path,note
            path,note = connectors[key]
            if path is not None:
                score = scores[i][0]+1,scores[i][1]+path.length
                if score<scores.get(j,(np.inf,np.inf)):
                    scores[j] = score
                    parents[j] = i,path,note
    target = len(positions)-1
    if target not in scores:
        return None,dict(reason='no_clear_source_pose_chain',candidates=candidates,waypoints=len(positions))
    parts,notes = [],[]
    while target:
        target,path,note = parents[target]
        parts.append(shapely.get_coordinates(path))
        notes.append(note)
    xy = np.vstack(parts[::-1])
    xy = xy[np.r_[True,np.linalg.norm(np.diff(xy,axis=0),axis=1)>1e-10]]
    path = shapely.LineString(xy)
    if not shapely.is_simple(path):
        return None,dict(reason='self_intersecting_rounded_route')
    radii = [n['minimum_radius_m'] for n in notes if n.get('minimum_radius_m') is not None]
    samples = sum(n['vehicle_sweep']['sampled_poses'] for n in notes)
    return path,dict(profile='G2-route',waypoints=len(positions),segments=len(parts),candidates=candidates,
                     lookahead=lookahead,length_m=path.length,
                     minimum_radius_m=min(radii) if radii else None,endpoint_curvature_per_m=0.,
                     vehicle_sweep=dict(fits_surface=True,sampled_poses=samples),pieces=notes)


def source_route_connector(entry,exit,route,surface,vehicle,error=.025,*,connectors=None):
    raw = shapely.get_coordinates(route)
    directions = raw[[1,-1]]-raw[[0,-2]]
    directions /= np.linalg.norm(directions,axis=1)[:,None]
    if min(directions[0]@entry['heading'],directions[1]@exit['heading'])<=0:
        return None,dict(reason='source_route_port_heading_mismatch')
    path,note = spline_route_candidates(entry,exit,route,surface,vehicle,error)
    if path is not None:
        return path,note
    if len(raw)<3:
        return None,dict(reason='source_route_has_no_intermediate_corner')
    tried = set()
    connectors = {} if connectors is None else connectors
    for tolerance in (error,error*2,error*4,error*8,error*16):
        interior = shapely.LineString(raw[1:-1]).simplify(tolerance) if len(raw)>3 else shapely.Point(raw[1])
        points = np.vstack((raw[0],shapely.get_coordinates(interior),raw[-1]))
        key = points.tobytes()
        if key in tried:
            continue
        tried.add(key)
        path,note = rounded_route(points,surface,vehicle,error,connectors=connectors,ports=(entry,exit))
        if path is not None:
            return path,dict(simplification_m=tolerance,**note)
    return None,note
