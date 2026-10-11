"""Build bounded roadbeds from source-following chains of analytic pose curves."""

import numpy as np
import shapely

from infrastructure_clothoid_turns import fit_symmetric_turns
from infrastructure_curvature_paths import fit_curvature_paths
from infrastructure_junction_surfaces import same_island_winding
from infrastructure_junction_turns import tangent_intersection
from infrastructure_route_turns import rounded_route
from infrastructure_turn_roadbeds import construct_turn_roadbed,roadbed_site_check,turn_roadbed
from infrastructure_vehicle_sweep import turn_body_check


def roadbed_piece(note):
    entry,exit=(dict(position=np.array(note[key]['position']),heading=np.array(note[key]['heading']))
                for key in ('entry_pose','exit_pose'))
    start,end=(np.array([np.arctan2(p['heading'][1],p['heading'][0])]) for p in (entry,exit))
    if note['profile']=='G2-polynomial':
        fit=fit_curvature_paths(entry['position'][None,:],exit['position'][None,:],start,end)
    elif note['profile']=='G2':
        node,reason=tangent_intersection(entry,exit)
        assert reason is None
        trim=min(np.linalg.norm(node-entry['position']),np.linalg.norm(exit['position']-node))*note['factor']
        fit=fit_symmetric_turns(node,np.array([trim]),start,end,transition_fraction=note['transition_fraction'])
    else:
        raise ValueError('unknown source roadbed curve')
    assert fit['valid'][0]
    return dict(fit=fit,begin=entry['position'],end=exit['position'],profile=note['profile'])


def roadbed_body_check(surface,pieces,vehicle,error,maximum_poses=4096):
    remaining=maximum_poses
    reports=[]
    for piece in pieces:
        body,_=turn_body_check(surface,piece['fit'],piece['begin'],piece['end'],piece['profile'],
                              vehicle,error,remaining)
        reports.append(body)
        remaining-=body.get('sampled_poses',0)
        if not body['fits_surface']:
            return dict(fits_surface=False,reason=body.get('reason','constructed_roadbed_does_not_support_body'),
                        pieces=reports,sampled_poses=maximum_poses-remaining)
    return dict(fits_surface=True,pieces=reports,sampled_poses=maximum_poses-remaining)


def construct_source_roadbed(entry,exit,surface,reservations,source_route,vehicle,error=.025):
    if not np.isfinite(error) or error<=0 or vehicle['widthM']*.5<=2*error:
        raise ValueError('invalid source-roadbed tolerance')
    clearance=2*error
    expansion=shapely.buffer(surface,vehicle['widthM']*.5-clearance,quad_segs=8)
    nearby=shapely.intersection(reservations,shapely.buffer(expansion.envelope,clearance,quad_segs=8))
    protected=shapely.buffer(nearby.difference(surface),clearance,quad_segs=8)
    domain=surface.union(expansion.difference(protected))
    path,note=rounded_route(shapely.get_coordinates(source_route),domain,vehicle,error,
                ports=(entry,exit),successor_offsets=(1,2,3,4,8,12,16),winding_surface=surface)
    row=dict(profile='G2-route',accepted=False,search=note)
    if path is None:
        return None,row|dict(reason=note['reason'])
    if not same_island_winding(path,source_route,surface):
        return None,row|dict(reason='roadbed_changes_directed_source_route')
    pieces=[roadbed_piece(n) for n in note['pieces'][::-1]]
    supports=[]
    for piece in pieces:
        support,audit=turn_roadbed(piece['fit'],piece['begin'],piece['end'],vehicle,error,profile=piece['profile'])
        if support is None:
            return None,row|dict(reason=audit['reason'])
        supports.append(support)
    roadbed=shapely.union_all(supports)
    planned,addition,reason=roadbed_site_check(roadbed,surface,reservations,vehicle,error)
    if reason is not None:
        return None,row|dict(reason=reason)
    body=roadbed_body_check(planned,pieces,vehicle,error)
    if not body['fits_surface']:
        return None,row|dict(reason=body['reason'],vehicle_sweep=body)
    row.update(accepted=True,reason='constructed_source_roadbed',added_area_m2=float(addition.area),
               minimum_radius_m=note['minimum_radius_m'],length_m=path.length,vehicle_sweep=body)
    return dict(surface=planned,roadbed=roadbed,addition=addition,path=path,pieces=pieces,report=row),row


def construct_route_roadbed(entry,exit,surface,reservations,source_route,vehicle,error=.025):
    candidate,trials=construct_turn_roadbed(entry,exit,surface,reservations,source_route,vehicle,error)
    if candidate is None:
        candidate,note=construct_source_roadbed(entry,exit,surface,reservations,source_route,vehicle,error)
        trials.append(note)
    return candidate,trials
