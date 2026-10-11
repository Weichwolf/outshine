"""Construct conservative roadbeds from bounded quintic Hermite fits."""

import numpy as np
import shapely

from infrastructure_hermite_route_fitting import fitted_route_candidates
from infrastructure_junction_surfaces import same_island_winding
from infrastructure_spline_routes import curve_bounds,body_acceleration_bounds,spline_sampler,checked_spline_path
from infrastructure_turn_roadbeds import roadbed_site_check,roadbed_lateral_reach,geometry_precision
from infrastructure_vehicle_sweep import body_outlines


def spline_roadbed(controls,vehicle,error,maximum_segments=255):
    minimum,speed,curvature=curve_bounds(controls)
    if not np.all(np.isfinite(curvature)) or curvature.max()*vehicle['minimumRearAxleRadiusM']>1+1e-12:
        return None,dict(reason='spline_curvature_bound_exceeds_vehicle')
    radius=np.hypot(max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']),vehicle['widthM']*.5)
    _,acceleration=body_acceleration_bounds(controls,minimum,speed,curvature,radius)
    counts=np.maximum(1,np.ceil(np.sqrt(acceleration/(8*error))))
    if not np.all(np.isfinite(counts)) or counts.sum()>maximum_segments:
        return None,dict(reason='roadbed_sampling_budget_exhausted')
    counts=counts.astype(int)
    owners=np.repeat(np.arange(len(controls)),counts)
    start=np.concatenate([np.arange(n)/n for n in counts])
    end=np.concatenate([np.arange(1,n+1)/n for n in counts])
    evaluate=spline_sampler(controls)
    outlines=[]
    for stations in (start,end):
        positions,headings=evaluate(owners,stations)
        outlines.append(body_outlines(positions,headings,vehicle['widthM']*.5,
                                      vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']))
    hulls=shapely.convex_hull(shapely.multipoints(np.concatenate(outlines,axis=1)))
    deviation=acceleration[owners]/(8*counts[owners]**2)
    support=shapely.union_all(shapely.buffer(hulls,(deviation+2*error)/np.cos(np.pi/32),quad_segs=8))
    return support,dict(hull_intervals=int(counts.sum()),sampled_body_poses=int(counts.sum()*2),
                        sweep_deviation_bound_m=float(deviation.max()),construction_clearance_m=2*error)


def construct_fitted_roadbed(entry,exit,surface,reservations,source_route,vehicle,error=.025,maximum_calls=1024):
    if error<=0 or not np.isfinite(error) or maximum_calls<1:
        raise ValueError('invalid fitted roadbed budget')
    calls,trials=0,[]
    for reach in sorted({vehicle['widthM']*.5,roadbed_lateral_reach(vehicle)}):
        if calls>=maximum_calls:break
        expansion=shapely.buffer(surface,reach-2*error,quad_segs=8)
        nearby=reservations.intersection(expansion.envelope.buffer(2*error))
        protected=shapely.buffer(nearby.difference(surface),2*error,quad_segs=8)
        domain=surface.union(expansion.difference(protected))
        path,note=fitted_route_candidates(entry,exit,source_route,domain,vehicle,error,
                                         maximum_calls=maximum_calls-calls,include_controls=True)
        calls+=note['residual_calls']
        row=dict(note,accepted=False,search_reach_m=reach,residual_calls=calls,maximum_residual_calls=maximum_calls)
        trials.append(row)
        if path is None:continue
        controls=np.array(row.pop('curve_controls'));note.pop('curve_controls')
        if not same_island_winding(path,source_route,surface):
            row['reason']='roadbed_changes_directed_source_route';continue
        existing,_=checked_spline_path(controls,surface,vehicle,error)
        roadbed=shapely.Polygon();planned=surface;addition=shapely.Polygon()
        if existing is None:
            roadbed,audit=spline_roadbed(controls,vehicle,error)
            row['construction']=audit
            if roadbed is None:
                row['reason']=audit['reason'];continue
            quantum=geometry_precision(surface)
            base,reserved,support=shapely.set_precision(np.array([surface,reservations,roadbed],dtype=object),quantum)
            trimmed=support.difference(reserved)
            row['trimmed_support_m2']=float(support.area-trimmed.area)
            row['coordinate_precision_m']=quantum
            roadbed=trimmed
            planned,addition,reason=roadbed_site_check(roadbed,base,reserved,vehicle,error)
            if reason is not None:
                row['reason']=reason;continue
        checked,proof=checked_spline_path(controls,planned,vehicle,error)
        if checked is None:
            row.update(reason=proof['reason'],published_candidate_check=proof);continue
        fit=dict(controls=controls,maximum_curvature_per_m=np.array([proof['curvature_bound_per_m']]))
        piece=dict(profile='G2-quintic-spline',fit=fit,begin=entry['position'],end=exit['position'])
        row.update(accepted=True,reason='constructed_fitted_roadbed',added_area_m2=float(addition.area),
                   vehicle_sweep=proof['vehicle_sweep'])
        return dict(surface=planned,roadbed=roadbed,addition=addition,path=checked,pieces=[piece],report=row),row
    return None,dict(reason='no_clear_fitted_roadbed',residual_calls=calls,maximum_residual_calls=maximum_calls,
                     trials=trials)
