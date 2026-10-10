"""Fit bounded continuous-curvature turns between permitted real lane poses."""

import numpy as np

from infrastructure_turn_paths import clearance_turns


def tangent_intersection(entry,exit,tolerance=.025):
    begin,end = entry['position'],exit['position']
    incoming,outgoing = entry['heading'],exit['heading']
    displacement = end-begin
    cross = lambda a,b:a[0]*b[1]-a[1]*b[0]
    determinant = cross(incoming,outgoing)
    if abs(determinant)<=1e-8:
        if incoming@outgoing>0 and displacement@incoming>0 and abs(cross(displacement,incoming))<=1e-7:
            return (begin+end)*.5,None
        return None,'requires_inflected_or_return_turn'
    before,after = cross(displacement,outgoing)/determinant,cross(incoming,displacement)/determinant
    if before<=0 or after<=0:
        return None,'requires_asymmetric_or_inflected_turn'
    return begin+before*incoming,None


def connect_lanes(entry,exit,surface,vehicle,error=.025):
    node,reason = tangent_intersection(entry,exit,error)
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
