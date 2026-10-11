"""Construct bounded road surfaces from analytic vehicle turns and site reservations."""

import numpy as np
import shapely

from infrastructure_clothoid_turns import fit_symmetric_turns,sample_symmetric_turns
from infrastructure_junction_surfaces import same_island_winding
from infrastructure_junction_turns import tangent_intersection
from infrastructure_vehicle_sweep import body_outlines,turn_body_check


def holes(surface):
    return np.array([shapely.Polygon(r) for p in shapely.get_parts(surface) for r in p.interiors],dtype=object)


def preserves_coverage(before,after):
    if shapely.covers(after,before):
        return True
    scale=max(1.,float(np.abs(shapely.get_coordinates(before)).max(initial=0)))
    roundoff=64*np.finfo(float).eps*scale
    lost=before.difference(after)
    return lost.area<=roundoff*before.length and shapely.covers(shapely.buffer(after,roundoff,quad_segs=8),before)


def preserves_voids(before,after):
    if len(shapely.get_parts(before))!=len(shapely.get_parts(after)):
        return False
    original,planned=holes(before),holes(after)
    if len(original)!=len(planned):
        return False
    if not len(original):
        return True
    markers=shapely.point_on_surface(original)
    pairs=shapely.STRtree(planned).query(markers,predicate='within')
    return len(pairs[0])==len(original) and len(np.unique(pairs[0]))==len(original) and len(np.unique(pairs[1]))==len(planned)


def turn_roadbed(fit,begin,end,vehicle,error,maximum_segments=255):
    curvature,length=float(fit['maximum_curvature_per_m'][0]),float(fit['length_m'][0])
    rate=float(fit['maximum_curvature_rate_per_m2'][0])
    radius=np.hypot(max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']),vehicle['widthM']*.5)
    acceleration=(curvature+radius*(rate+curvature**2))*length**2
    segments=max(1,int(np.ceil(np.sqrt(acceleration/(8*error)))))
    if segments>maximum_segments:
        return None,dict(reason='roadbed_sampling_budget_exhausted')
    positions,headings=sample_symmetric_turns(fit,np.linspace(0,1,segments+1))[:2]
    positions,headings=positions[0],headings[0]
    outlines=body_outlines(positions,headings,vehicle['widthM']*.5,
                          vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    hulls=shapely.convex_hull(shapely.multipoints(np.concatenate((outlines[:-1],outlines[1:]),axis=1)))
    deviation=acceleration/(8*segments**2)
    curved=shapely.buffer(hulls,(deviation+2*error)/np.cos(np.pi/32),quad_segs=8)
    approaches=[]
    for a,b,angle in ((begin,positions[0],headings[0]),(positions[-1],end,headings[-1])):
        bodies=body_outlines(np.array([a,b]),np.full(2,angle),vehicle['widthM']*.5,
                            vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
        prism=shapely.convex_hull(shapely.MultiPoint(bodies.reshape(-1,2)))
        approaches.append(shapely.buffer(prism,2*error/np.cos(np.pi/32),quad_segs=8))
    return shapely.union_all([*curved,*approaches]),dict(hull_intervals=segments+2,
            sampled_body_poses=segments+5,sweep_deviation_bound_m=float(deviation),
            construction_clearance_m=2*error)


def construct_turn_roadbed(entry,exit,surface,reservations,source_route,vehicle,error=.025):
    if not np.isfinite(error) or error<=0:
        raise ValueError('invalid roadbed tolerance')
    node,reason=tangent_intersection(entry,exit)
    if reason is not None:
        return None,[dict(accepted=False,reason=reason)]
    start,finish=(np.array([np.arctan2(p['heading'][1],p['heading'][0])]) for p in (entry,exit))
    available=min(np.linalg.norm(node-entry['position']),np.linalg.norm(exit['position']-node))
    domain=shapely.buffer(surface,vehicle['widthM']*.5,quad_segs=8)
    candidates,trials=[],[]
    for transition in (.5,.25,.125):
        row=dict(transition_fraction=transition,accepted=False)
        trials.append(row)
        unit=fit_symmetric_turns(node,np.array([1.]),start,finish,transition_fraction=transition)
        trim=float(unit['maximum_curvature_per_m'][0])*vehicle['minimumRearAxleRadiusM']*(1+1e-9)
        if not unit['valid'][0] or trim>available:
            row['reason']='turn_exceeds_fixed_approaches'
            continue
        fit=fit_symmetric_turns(node,np.array([trim]),start,finish,transition_fraction=transition)
        if not fit['valid'][0]:
            row['reason']='invalid_analytic_turn'
            continue
        roadbed,note=turn_roadbed(fit,entry['position'],exit['position'],vehicle,error)
        row.update(note)
        if roadbed is None:
            continue
        addition=shapely.difference(roadbed,surface)
        row['added_area_m2']=float(addition.area)
        if not shapely.covers(domain,roadbed):
            row['reason']='roadbed_exceeds_local_flare_budget'
            continue
        if addition.intersection(reservations).area>0:
            row['reason']='roadbed_intersects_reserved_space'
            continue
        planned=shapely.union_all([surface,roadbed])
        if not planned.is_valid or not preserves_voids(surface,planned) or not preserves_coverage(surface,planned):
            row['reason']='roadbed_changes_void_topology'
            continue
        curvature,length=float(fit['maximum_curvature_per_m'][0]),float(fit['length_m'][0])
        count=max(1,int(np.ceil(length*np.sqrt(curvature/(8*error)))))
        if count>255:
            row['reason']='turn_path_sampling_budget_exhausted'
            continue
        xy=sample_symmetric_turns(fit,np.linspace(0,1,count+1))[0][0]
        points=np.vstack((entry['position'],xy,exit['position']))
        points=points[np.r_[True,np.linalg.norm(np.diff(points,axis=0),axis=1)>1e-10]]
        path=shapely.LineString(points)
        if not path.is_simple or not same_island_winding(path,source_route,surface):
            row['reason']='roadbed_changes_directed_source_route'
            continue
        body,_=turn_body_check(planned,fit,entry['position'],exit['position'],'G2',vehicle,error)
        row.update(vehicle_sweep=body,minimum_radius_m=1/curvature if curvature>0 else None,
                   arc_segments=count,endpoint_curvature_per_m=0.,length_m=length)
        if not body['fits_surface']:
            row['reason']=body.get('reason','constructed_roadbed_does_not_support_body')
            continue
        row.update(accepted=True,reason='constructed_local_roadbed',profile='G2')
        candidates.append(dict(surface=planned,addition=addition,path=path,fit=fit,report=row))
    best=min(candidates,key=lambda c:(c['report']['added_area_m2'],c['report']['transition_fraction'])) if candidates else None
    return best,trials
