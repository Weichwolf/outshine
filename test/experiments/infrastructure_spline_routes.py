"""Bounded quintic source-route interpolation with conservative Bernstein envelopes."""

from math import comb

import numpy as np
import shapely
from scipy.interpolate import BPoly,PPoly,make_interp_spline

from infrastructure_vehicle_sweep import sweep_check
from infrastructure_route_fairing import fair_route
from infrastructure_junction_surfaces import same_island_winding


def split_controls(controls):
    level = controls
    left,right = [level[:,0]],[level[:,-1]]
    while level.shape[1]>1:
        level = (level[:,:-1]+level[:,1:])*.5
        left.append(level[:,0]);right.append(level[:,-1])
    return np.stack((np.stack(left,axis=1),np.stack(right[::-1],axis=1)),axis=1).reshape(-1,controls.shape[1],2)


def curve_bounds(controls):
    degree = controls.shape[1]-1
    velocity = degree*np.diff(controls,axis=1)
    acceleration = (degree-1)*np.diff(velocity,axis=1)
    chord = controls[:,-1]-controls[:,0]
    length = np.linalg.norm(chord,axis=1)
    direction = np.divide(chord,length[:,None],out=np.zeros_like(chord),where=length[:,None]>0)
    minimum_speed = np.einsum('nki,ni->nk',velocity,direction).min(axis=1)
    maximum_speed = np.linalg.norm(velocity,axis=2).max(axis=1)
    a,b = degree-1,degree-2
    cross = np.zeros((len(controls),a+b+1))
    for i in range(a+1):
        for j in range(b+1):
            weight = comb(a,i)*comb(b,j)/comb(a+b,i+j)
            cross[:,i+j] += weight*(velocity[:,i,0]*acceleration[:,j,1]-velocity[:,i,1]*acceleration[:,j,0])
    curvature = np.divide(np.abs(cross).max(axis=1),minimum_speed**3,
                          out=np.full(len(controls),np.inf),where=minimum_speed>1e-10)
    return minimum_speed,maximum_speed,curvature


def evaluate_controls(controls,stations):
    degree = controls.shape[1]-1
    basis = np.stack([comb(degree,i)*stations**i*(1-stations)**(degree-i) for i in range(degree+1)],axis=1)
    return np.einsum('nk,nki->ni',basis,controls)


def spline_controls(points,entry,exit):
    distance = np.linalg.norm(np.diff(points,axis=0),axis=1)
    if np.any(distance<=1e-7):
        return None
    parameters = np.r_[0,np.cumsum(distance)]
    boundary = ([(1,entry['heading']),(2,np.zeros(2))],[(1,exit['heading']),(2,np.zeros(2))])
    spline = make_interp_spline(parameters,points,k=5,bc_type=boundary)
    pieces = [BPoly.from_power_basis(PPoly.from_spline((spline.t,spline.c[:,axis],spline.k))) for axis in range(2)]
    positive = np.diff(pieces[0].x)>0
    controls = np.stack([p.c[:,positive].T for p in pieces],axis=-1)
    for _ in range(3):
        controls = split_controls(controls)
    return controls


def spline_route(entry,exit,route,surface,vehicle,error=.025,maximum_poses=4096,spacing=None,fairing_weight=0.):
    if spacing is None:
        raw = shapely.get_coordinates(route)
    else:
        count = max(2,int(np.ceil(route.length/spacing))+1)
        if count>66:
            return None,dict(reason='spline_knot_budget_exhausted')
        raw = shapely.get_coordinates(shapely.line_interpolate_point(route,np.linspace(0,route.length,count)))
    if len(raw)>66:
        return None,dict(reason='spline_knot_budget_exhausted')
    points = raw.copy()
    points[[0,-1]] = [entry['position'],exit['position']]
    points = fair_route(points,fairing_weight)
    controls = spline_controls(points,entry,exit)
    if controls is None:
        return None,dict(reason='collapsed_spline_interval')
    minimum,speed,curvature = curve_bounds(controls)
    bound = float(curvature.max())
    if not np.isfinite(bound) or bound*vehicle['minimumRearAxleRadiusM']>1+1e-12:
        return None,dict(reason='spline_curvature_bound_exceeds_vehicle',maximum_curvature_bound=bound if np.isfinite(bound) else None)
    radius = np.hypot(max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']),vehicle['widthM']*.5)
    spacing = 2*error/(1+bound*radius)
    counts = np.maximum(1,np.ceil(speed/spacing).astype(int))
    if counts.sum()+len(counts)>maximum_poses:
        return None,dict(reason='spline_pose_budget_exhausted')
    owners = np.repeat(np.arange(len(controls)),counts+1)
    stations = np.concatenate([np.linspace(0,1,n+1) for n in counts])
    xy = evaluate_controls(controls[owners],stations)
    velocity = evaluate_controls(5*np.diff(controls[owners],axis=1),stations)
    headings = np.arctan2(velocity[:,1],velocity[:,0])
    body,_ = sweep_check(surface,xy,headings,bound,float((speed/counts).max()),
                        vehicle['widthM']*.5,vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    if not body['fits_surface']:
        return None,dict(reason='spline_vehicle_body_outside_surface')
    xy = xy[np.r_[True,np.linalg.norm(np.diff(xy,axis=0),axis=1)>1e-10]]
    path = shapely.LineString(xy)
    if not shapely.is_simple(path):
        return None,dict(reason='self_intersecting_spline_route')
    return path,dict(profile='G2-quintic-spline',length_m=path.length,vehicle_sweep=body,
                     minimum_radius_m=1/bound if bound>0 else None,curvature_bound_per_m=bound,
                     endpoint_curvature_per_m=0.)


def spline_route_candidates(entry,exit,route,surface,vehicle,error):
    radius = vehicle['minimumRearAxleRadiusM']
    for spacing in (radius*2,radius,radius*3,radius*4,None):
        path,note = spline_route(entry,exit,route,surface,vehicle,error,spacing=spacing)
        if path is not None and same_island_winding(path,route,surface):
            return path,dict(note,source_spacing_m=spacing)
        if path is not None:
            note = dict(reason='spline_changes_source_island_winding')
    for spacing in (radius,radius*2):
        if route.length<=spacing:
            continue
        for weight in (.1,1.,10.,100.):
            path,note = spline_route(entry,exit,route,surface,vehicle,error,spacing=spacing,fairing_weight=weight)
            if path is not None:
                if same_island_winding(path,route,surface):
                    return path,dict(note,source_spacing_m=spacing,fairing_weight=weight)
                note = dict(reason='spline_changes_source_island_winding')
    return None,note
