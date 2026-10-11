"""Construct bounded road surfaces from analytic vehicle turns and site reservations."""

import numpy as np
import shapely

from infrastructure_clothoid_turns import fit_symmetric_turns
from infrastructure_curvature_paths import fit_curvature_paths
from infrastructure_junction_surfaces import same_island_winding
from infrastructure_junction_turns import tangent_intersection
from infrastructure_vehicle_sweep import body_outlines,turn_body_check,turn_sampler


def bounded_voids(surface):
    shells = shapely.polygons(shapely.get_exterior_ring(shapely.get_parts(surface)))
    parts = shapely.get_parts(shapely.union_all(shells).difference(surface))
    return parts[shapely.area(parts)>0]


def coordinate_roundoff(surface):
    scale=max(1.,float(np.abs(shapely.get_coordinates(surface)).max(initial=0)))
    return float(64*np.finfo(float).eps*scale)


def preserves_coverage(before,after):
    shapely.prepare(after)
    if shapely.covers(after,before):
        return True
    roundoff=coordinate_roundoff(before)
    lost=before.difference(after)
    return lost.area<=roundoff*before.length and shapely.covers(shapely.buffer(after,roundoff,quad_segs=8),before)


def preserves_voids(before,after):
    if len(shapely.get_parts(before))!=len(shapely.get_parts(after)):
        return False
    original,planned=bounded_voids(before),bounded_voids(after)
    if len(original)!=len(planned):
        return False
    if not len(original):
        return True
    pairs=shapely.STRtree(planned).query(original,predicate='intersects')
    overlaps=shapely.area(shapely.intersection(original[pairs[0]],planned[pairs[1]]))
    pairs=pairs[:,overlaps>0]
    return len(pairs[0])==len(original) and len(np.unique(pairs[0]))==len(original) and len(np.unique(pairs[1]))==len(planned)


def geometry_precision(surface):
    roundoff=coordinate_roundoff(surface)
    return float(2.**np.floor(np.log2(roundoff/4)))


def fill_roundoff_voids(before,after):
    original=shapely.STRtree(bounded_voids(before))
    patches=[]
    for hole in bounded_voids(after):
        neighbors=original.geometries[original.query(hole,predicate='intersects')]
        if any(hole.intersection(previous).area>0 for previous in neighbors):continue
        origin=shapely.get_coordinates(hole)[0]
        local=shapely.transform(hole,lambda points:points-origin)
        rectangle=shapely.minimum_rotated_rectangle(local)
        points=shapely.get_coordinates(rectangle)
        width=np.linalg.norm(np.diff(points,axis=0),axis=1).min() if rectangle.geom_type=='Polygon' else 0.
        if width<=coordinate_roundoff(before):patches.append(hole)
    return after if not patches else shapely.union_all([after,*patches])


def compose_roadbeds(surface,shape,roadbeds):
    precision=geometry_precision(surface)
    base,core,*supports=shapely.set_precision(np.array([surface,shape,*roadbeds],dtype=object),precision)
    planned=fill_roundoff_voids(base,shapely.union_all([base,*supports]))
    physical=core.union(planned.difference(base))
    repaired=fill_roundoff_voids(core,physical)
    planned=planned.union(repaired.difference(physical))
    return base,core,planned,repaired,precision


def published_roadbed_surface(mesh,base,core,planned,precision):
    vertices=mesh['vertices']
    assert np.array_equal(vertices,np.rint(vertices/precision)*precision)
    faces=shapely.polygons(vertices[mesh['indices']])
    actual=shapely.union_all(shapely.set_precision(faces,precision))
    complete=actual.intersection(planned).union(base.difference(core))
    parts=shapely.get_parts(complete)
    polygons=shapely.get_parts(parts[np.isin(shapely.get_type_id(parts),[3,6])])
    surface=shapely.MultiPolygon(polygons.tolist()) if len(polygons)!=1 else polygons[0]
    assert surface.area==complete.area
    return surface,actual


def turn_roadbed(fit,begin,end,vehicle,error,maximum_segments=255,profile='G2'):
    curvature,length=float(fit['maximum_curvature_per_m'][0]),float(fit['length_m'][0])
    rate=float(fit['maximum_curvature_rate_per_m2'][0])
    radius=np.hypot(max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']),vehicle['widthM']*.5)
    acceleration=(curvature+radius*(rate+curvature**2))*length**2
    segments=max(1,int(np.ceil(np.sqrt(acceleration/(8*error)))))
    if segments>maximum_segments:
        return None,dict(reason='roadbed_sampling_budget_exhausted')
    positions,headings=turn_sampler(profile)(fit,np.linspace(0,1,segments+1))[:2]
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


def roadbed_lateral_reach(vehicle):
    half=vehicle['widthM']*.5
    radius=vehicle['minimumRearAxleRadiusM']
    length=max(vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    return half+length**2/(np.hypot(radius+half,length)+radius+half)


def roadbed_site_check(roadbed,surface,reservations,vehicle,error):
    addition=shapely.difference(roadbed,surface)
    a,b,c,d=roadbed.bounds
    reach=roadbed_lateral_reach(vehicle)
    padding=reach+error
    local=shapely.intersection(surface,shapely.box(a-padding,b-padding,c+padding,d+padding))
    domain=shapely.buffer(local,reach,quad_segs=8)
    if not shapely.covers(domain,roadbed):
        return None,addition,'roadbed_exceeds_local_flare_budget'
    if addition.intersection(reservations).area>0:
        return None,addition,'roadbed_intersects_reserved_space'
    planned=shapely.union_all([surface,roadbed])
    if not planned.is_valid or not preserves_voids(surface,planned) or not preserves_coverage(surface,planned):
        return None,addition,'roadbed_changes_void_topology'
    return planned,addition,None


def roadbed_curves(entry,exit,vehicle):
    node,reason=tangent_intersection(entry,exit)
    start,finish=(np.array([np.arctan2(p['heading'][1],p['heading'][0])]) for p in (entry,exit))
    if reason is not None:
        yield None,dict(profile='G2',accepted=False,reason=reason)
    else:
        available=min(np.linalg.norm(node-entry['position']),np.linalg.norm(exit['position']-node))
        for transition in (.5,.25,.125):
            row=dict(profile='G2',transition_fraction=transition,accepted=False)
            unit=fit_symmetric_turns(node,np.array([1.]),start,finish,transition_fraction=transition)
            trim=float(unit['maximum_curvature_per_m'][0])*vehicle['minimumRearAxleRadiusM']*(1+1e-9)
            if not unit['valid'][0] or trim>available:
                yield None,row|dict(reason='turn_exceeds_fixed_approaches')
                continue
            fit=fit_symmetric_turns(node,np.array([trim]),start,finish,transition_fraction=transition)
            yield (fit if fit['valid'][0] else None),row|({} if fit['valid'][0] else dict(reason='invalid_analytic_turn'))
    fit=fit_curvature_paths(entry['position'][None,:],exit['position'][None,:],start,finish,
                            minimum_radius=vehicle['minimumRearAxleRadiusM'])
    row=dict(profile='G2-polynomial',accepted=False,heading_turns=int(fit['heading_turns'][0]))
    if not fit['valid'][0]:
        yield None,row|dict(reason='polynomial_pose_fit_failed')
    elif fit['maximum_curvature_per_m'][0]*vehicle['minimumRearAxleRadiusM']>1+1e-12:
        yield None,row|dict(reason='polynomial_turn_radius_too_small')
    else:
        yield fit,row


def construct_turn_roadbed(entry,exit,surface,reservations,source_route,vehicle,error=.025):
    if not np.isfinite(error) or error<=0:
        raise ValueError('invalid roadbed tolerance')
    candidates,trials=[],[]
    for fit,row in roadbed_curves(entry,exit,vehicle):
        trials.append(row)
        if fit is None:
            continue
        profile=row['profile']
        roadbed,note=turn_roadbed(fit,entry['position'],exit['position'],vehicle,error,profile=profile)
        row.update(note,flare_bound_m=roadbed_lateral_reach(vehicle))
        if roadbed is None:
            continue
        planned,addition,reason=roadbed_site_check(roadbed,surface,reservations,vehicle,error)
        row['added_area_m2']=float(addition.area)
        if reason is not None:
            row['reason']=reason
            continue
        curvature,length=float(fit['maximum_curvature_per_m'][0]),float(fit['length_m'][0])
        count=max(1,int(np.ceil(length*np.sqrt(curvature/(8*error)))))
        if count>255:
            row['reason']='turn_path_sampling_budget_exhausted'
            continue
        xy=turn_sampler(profile)(fit,np.linspace(0,1,count+1))[0][0]
        points=np.vstack((entry['position'],xy,exit['position']))
        points=points[np.r_[True,np.linalg.norm(np.diff(points,axis=0),axis=1)>1e-10]]
        path=shapely.LineString(points)
        if not path.is_simple or not same_island_winding(path,source_route,surface):
            row['reason']='roadbed_changes_directed_source_route'
            continue
        body,_=turn_body_check(planned,fit,entry['position'],exit['position'],profile,vehicle,error)
        row.update(vehicle_sweep=body,minimum_radius_m=1/curvature if curvature>0 else None,
                   arc_segments=count,endpoint_curvature_per_m=0.,length_m=length)
        if not body['fits_surface']:
            row['reason']=body.get('reason','constructed_roadbed_does_not_support_body')
            continue
        row.update(accepted=True,reason='constructed_local_roadbed')
        candidates.append(dict(surface=planned,roadbed=roadbed,addition=addition,path=path,fit=fit,report=row,
                    pieces=[dict(fit=fit,begin=entry['position'],end=exit['position'],profile=profile)]))
    best=min(candidates,key=lambda c:(c['report']['added_area_m2'],c['report'].get('transition_fraction',1.))) if candidates else None
    return best,trials
