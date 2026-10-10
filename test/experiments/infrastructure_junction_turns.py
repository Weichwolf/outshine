"""Fit bounded continuous-curvature turns between permitted real lane poses."""

import numpy as np
import shapely

from infrastructure_turn_paths import clearance_turns
from infrastructure_curvature_paths import fit_curvature_paths,sample_curvature_paths
from infrastructure_vehicle_sweep import turn_body_check


def tangent_intersection(entry,exit,tolerance=1e-7):
    begin,end = entry['position'],exit['position']
    incoming,outgoing = entry['heading'],exit['heading']
    displacement = end-begin
    cross = lambda a,b:a[0]*b[1]-a[1]*b[0]
    determinant = cross(incoming,outgoing)
    if abs(determinant)<=1e-8:
        if incoming@outgoing>0 and displacement@incoming>0 and abs(cross(displacement,incoming))<=tolerance:
            return (begin+end)*.5,None
        return None,'requires_inflected_or_return_turn'
    before,after = cross(displacement,outgoing)/determinant,cross(incoming,displacement)/determinant
    if before<=0 or after<=0:
        return None,'requires_asymmetric_or_inflected_turn'
    return begin+before*incoming,None


def clothoid_connector(entry,exit,surface,vehicle,error):
    node,reason = tangent_intersection(entry,exit)
    if reason is not None:
        return None,dict(reason=reason)
    paths,reports = [],[]
    for transition in (.5,.25,.125):
        trial,notes = clearance_turns(node,entry['position'],exit['position'],surface,vehicle=vehicle,
                                     error=error,transition_fraction=transition)
        paths.extend(trial)
        reports.extend(notes)
        valid = [i for i,r in enumerate(reports) if r['fits_surface']]
        if valid:
            best = min(valid,key=lambda i:reports[i]['length_m'])
            return paths[best],dict(node=node.tolist(),**reports[best])
    return None,dict(reason='no_clear_body_within_radius_and_sampling_budget',candidates=len(paths),
                     body_checked_candidates=sum(r['vehicle_sweep'] is not None for r in reports))


def curvature_connector(entry,exit,surface,vehicle,error):
    begin,end = entry['position'],exit['position']
    initial,final = (np.array([np.arctan2(p['heading'][1],p['heading'][0])]) for p in (entry,exit))
    fit = fit_curvature_paths(begin[None,:],end[None,:],initial,final)
    if not fit['valid'][0]:
        return None,dict(reason='polynomial_pose_fit_failed')
    curvature,length = float(fit['maximum_curvature_per_m'][0]),float(fit['length_m'][0])
    if curvature*vehicle['minimumRearAxleRadiusM']>1+1e-12:
        return None,dict(reason='polynomial_turn_radius_too_small')
    count = max(1,int(np.ceil(length*np.sqrt(curvature/(8*error)))))
    if count>255:
        return None,dict(reason='polynomial_curve_sampling_budget_exhausted')
    xy = sample_curvature_paths(fit,np.linspace(0,1,count+1))[0][0]
    path = shapely.LineString(xy)
    if not shapely.is_simple(path):
        return None,dict(reason='looping_direct_polynomial_turn')
    body,_ = turn_body_check(surface,fit,begin,end,'G2-polynomial',vehicle,error)
    if not body['fits_surface']:
        return None,dict(reason=body.get('reason','polynomial_vehicle_body_outside_surface'))
    return path,dict(profile='G2-polynomial',length_m=length,
                     minimum_radius_m=1/curvature if curvature>0 else None,arc_segments=count,
                     amplitude=float(fit['amplitude'][0]),closure_error_m=float(fit['closure_error_m'][0]),
                     endpoint_curvature_per_m=0.,vehicle_sweep=body)


def connect_lanes(entry,exit,surface,vehicle,error=.025):
    path,report = clothoid_connector(entry,exit,surface,vehicle,error)
    if path is not None:
        return path,report
    trial,note = curvature_connector(entry,exit,surface,vehicle,error)
    if trial is not None:
        return trial,note
    return None,dict(reason=report['reason'],polynomial_reason=note['reason'])
