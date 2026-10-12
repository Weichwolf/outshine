"""Publish and prove coupled roadbed and shared-cut corrections on real inputs."""

import argparse
import json
from pathlib import Path
import resource
import subprocess
import sys
import time

import numpy as np
import shapely

from transport_cut_setback_experiment import prepared_network,digest,implementation_files
from transport_cut_setbacks import restore_cut_plan,setback_network,external_frames_unchanged,serialize_cut_plan
from transport_directed_routes import directed_module_routes
from transport_flat_surfaces import shared_mesh
from transport_hermite_route_fitting import fitted_route_candidates
from transport_junction_surfaces import motor_surface,same_island_winding
from transport_junction_turns import connect_lanes
from transport_joint_roadbed_frames import joint_heading_trials
from transport_route_turns import rounded_source_route
from transport_shared_frames import frame_key
from transport_source_roadbeds import construct_route_roadbed,roadbed_piece,published_roadbed_body_check,sample_roadbed_piece
from transport_turn_roadbed_experiment import building_reservations,reservations
from transport_turn_roadbeds import compose_roadbeds,preserves_voids,preserves_coverage,published_roadbed_surface,roadbed_lateral_reach
from transport_vehicle_sweep import body_outlines
from transport_shared_cut_changes import changed_shared_cut
from transport_surface_regions import canonical_grid_region


def request_keys(poses,routes):
    keys=[frame_key(p)[:3] for p in poses]
    assert len(set(keys))==len(keys),'ambiguous source contact identity'
    return {(keys[a],keys[b]) for a,b in routes}


def existing_surface_curve(entry,exit,route,surface,vehicle,error,fit_existing=False):
    path,note=connect_lanes(entry,exit,surface,vehicle,error)
    if path is not None and same_island_winding(path,route,surface):
        poses=dict(entry_pose={k:entry[k].tolist() for k in ('position','heading')},
                   exit_pose={k:exit[k].tolist() for k in ('position','heading')})
        return dict(path=path,pieces=[roadbed_piece(note|poses)],roadbed=shapely.Polygon(),report=note)
    path,note=rounded_source_route(entry,exit,route,surface,vehicle,error,winding_surface=surface)
    if path is not None and same_island_winding(path,route,surface):
        return dict(path=path,pieces=[roadbed_piece(n) for n in note['pieces'][::-1]],
                    roadbed=shapely.Polygon(),report=note)
    if fit_existing:
        path,note=fitted_route_candidates(entry,exit,route,surface,vehicle,error,include_controls=True)
        if path is not None:
            fit=dict(controls=np.array(note.pop('curve_controls')),
                     maximum_curvature_per_m=np.array([note['curvature_bound_per_m']]))
            piece=dict(profile='G2-quintic-spline',fit=fit,begin=entry['position'],end=exit['position'])
            return dict(path=path,pieces=[piece],roadbed=shapely.Polygon(),report=note)
    return None


def roadbed_candidate(entry,exit,route,floor,reserved,vehicle,error,fit_existing,cache):
    context=(floor.wkb,shapely.get_precision(floor),reserved.wkb,shapely.get_precision(reserved),
             json.dumps(vehicle,sort_keys=True),error)
    candidates=cache['contexts'].setdefault(context,{})
    key=tuple(p[k].tobytes() for p in (entry,exit) for k in ('position','heading'))+(route.wkb,shapely.get_precision(route),fit_existing)
    if key in candidates:
        cache['hits']+=1
        return candidates[key]
    cache['misses']+=1
    candidate=existing_surface_curve(entry,exit,route,floor,vehicle,error,fit_existing)
    existing=candidate is not None;notes=[]
    if candidate is None: candidate,notes=construct_route_roadbed(entry,exit,floor,reserved,route,vehicle,error)
    result=candidate,existing,notes
    candidates[key]=result
    return result


def coupled_addition_rejection(floor,shape,supports,reserved,section_index,trial,owner,affected):
    base,core,planned,physical,_=compose_roadbeds(floor,shape,supports)
    if not preserves_voids(base,planned) or not preserves_voids(core,physical):return 'combined_void_topology'
    addition=planned.difference(base)
    if addition.intersection(reserved).area>1e-8:return 'reservation_overlap'
    if changed_shared_cut(base,addition,section_index):return 'changed_shared_cut'
    ids=trial.mesh[1].query(addition,predicate='intersects')
    owners=set(map(int,trial.product['face_owners'][ids]))|affected
    if any(addition.intersection(trial.shape(m)).area>1e-8 for m in owners if m!=owner):return 'owner_overlap'
    return None


def unresolved_route(owner,pair,poses,route,vehicle,reason,details=None):
    positions=np.array([poses[i]['position'] for i in pair])
    headings=np.array([np.arctan2(*poses[i]['heading'][::-1]) for i in pair])
    bodies=body_outlines(positions,headings,vehicle['widthM']*.5,
                        vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    return dict(module=owner,pair=pair,entry=pair[0],exit=pair[1],accepted=False,
                request=next(iter(request_keys(poses,[pair]))),reason=reason,details=details,
                source_path=shapely.get_coordinates(route).tolist(),endpoint_bodies=bodies.tolist())


def construct_module(network,trial,owner,sections,affected,region,buildings,section_index,required,cache=None):
    if cache is None:cache=dict(contexts={},hits=0,misses=0)
    old_poses,_=network.poses(owner,{})
    poses,_=trial.poses(owner,{})
    old_routes,_=directed_module_routes(network.graph,old_poses,network.shape(owner),network.tree)
    routes,_=directed_module_routes(trial.graph,poses,trial.shape(owner),trial.tree)
    assert request_keys(old_poses,old_routes)==request_keys(poses,routes),'source requests changed'
    assert external_frames_unchanged(old_poses,poses,sections),'external frame changed'
    shape=trial.shape(owner)
    demand=shapely.union_all(list(routes.values())).envelope if routes else shape
    floor=motor_surface(demand,trial.graph,trial.tree,trial.mesh,trial.vehicle,
                        trial.curbs['corner_radius_m'],trial.curbs['arc_error_m'])
    reserved=buildings.union(reservations(trial,region).difference(floor))
    curves=[];unresolved=[]
    for pair,route in sorted(routes.items(),key=lambda item:(not request_keys(poses,[item[0]])<=required,item[0])):
        entry,exit=(poses[i] for i in pair)
        candidate,existing,notes=roadbed_candidate(entry,exit,route,floor,reserved,trial.vehicle,
                    trial.curbs['arc_error_m'],request_keys(poses,[pair])<=required,cache)
        if candidate is None:
            unresolved.append(unresolved_route(owner,pair,poses,route,trial.vehicle,
                                               'no_constructive_candidate',[n.get('reason') for n in notes]))
            continue
        supports=[*[c['candidate']['roadbed'] for c in curves],candidate['roadbed']]
        reason=coupled_addition_rejection(floor,shape,supports,reserved,section_index,trial,owner,affected)
        if reason:
            unresolved.append(unresolved_route(owner,pair,poses,route,trial.vehicle,reason))
            continue
        curves.append(dict(pair=pair,candidate=candidate,existing_surface=existing,source_route=route))
    assert required<=request_keys(poses,[c['pair'] for c in curves]),'previous routes lost'
    base,core,planned,physical,quantum=compose_roadbeds(floor,shape,[c['candidate']['roadbed'] for c in curves])
    assert preserves_voids(base,planned) and preserves_voids(core,physical),'changed void topology'
    addition=planned.difference(base)
    assert addition.intersection(reserved).area<=1e-8,'reservation overlap'
    assert not changed_shared_cut(base,addition,section_index),'changed shared cut'
    ids=trial.mesh[1].query(addition,predicate='intersects')
    owners=set(map(int,trial.product['face_owners'][ids]))|affected
    assert not any(addition.intersection(trial.shape(m)).area>1e-8 for m in owners if m!=owner),'owner overlap'
    print(json.dumps(dict(module=owner,requests=len(routes),constructed=len(curves),
                          existing_surface=sum(c['existing_surface'] for c in curves),
                          added_area_m2=addition.area)),flush=True)
    return dict(base=base,core=core,planned=planned,physical=physical,quantum=quantum,curves=curves,poses=poses,unresolved=unresolved,connections=len(routes))


def publish_coupled_roadbeds(products,trial,precision):
    quantum=min(product['quantum'] for product in products.values())
    for product in products.values():
        product['quantum']=quantum
        for key in ('base','core','planned','physical'):
            product[key]=canonical_grid_region(product[key],quantum)
    parts=[];owners=[]
    for owner,product in products.items():
        components=list(shapely.get_parts(product['physical']))
        parts.extend(components);owners.extend([owner]*len(components))
    parts=np.array(parts,dtype=object);owners=np.array(owners,dtype=int)
    physical=shapely.union_all(parts)
    mesh,audit=shared_mesh(physical,parts,np.ones(len(parts),dtype=int),precision)
    face_owners=owners[mesh['face_owners']]
    actuals={};proofs=[];surfaces=[]
    for owner,product in products.items():
        owned=dict(vertices=mesh['vertices'],indices=mesh['indices'][face_owners==owner])
        complete,actual=published_roadbed_surface(owned,product['base'],product['core'],
                                                 product['planned'],product['quantum'])
        assert preserves_coverage(product['base'],complete),'lost original public floor'
        assert preserves_voids(product['base'],complete) and preserves_voids(product['core'],actual)
        actuals[owner]=actual
        surfaces.append(dict(module=owner,public_surface_wkb=complete.wkb.hex(),physical_shape_wkb=actual.wkb.hex(),
                             coordinate_precision_m=product['quantum']))
        for curve in product['curves']:
            body=published_roadbed_body_check(complete,curve['candidate']['pieces'],trial.vehicle,trial.curbs['arc_error_m'])
            assert body['fits_surface'],(owner,curve['pair'],body)
            key=next(iter(request_keys(product['poses'],[curve['pair']])))
            proofs.append(dict(module=owner,pair=curve['pair'],request=key,body=body,
                pieces=curve['candidate']['pieces'],path=shapely.get_coordinates(curve['candidate']['path']).tolist(),
                source_route_wkb=curve['source_route'].wkb.hex()))
    for owner in products:
        ports=trial.junctions[owner] if owner in trial.junctions else [p for j,s in trial.corridors[owner]
                for p in trial.junctions[j] if p['section']==s]
        boundary=shapely.buffer(actuals[owner].boundary,1e-7)
        shapely.prepare(boundary)
        for port in ports:
            assert shapely.covers(boundary,port['line']),'lost shared section'
    return dict(vertices=mesh['vertices'],indices=mesh['indices'],face_owners=face_owners),audit,proofs,surfaces


def published_frames(products):
    frames={}
    for product in products.values():
        for pose in product['poses']:
            key=frame_key(pose)
            row={k:pose[k] for k in ('section','edge','direction','station','position','heading')}
            if key in frames:
                assert all(np.array_equal(frames[key][k],row[k]) for k in ('position','heading'))
            frames[key]=row
    return [frames[k] for k in sorted(frames)]


def diagnostic_trial(curve,product,vehicle,original_poses,original_routes):
    outlines=[]
    for piece in curve['candidate']['pieces']:
        xy,heading=sample_roadbed_piece(piece,np.linspace(0,1,65))
        outlines.append(body_outlines(xy,heading,vehicle['widthM']*.5,
                                     vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']))
    bodies=np.concatenate(outlines)
    outside=shapely.area(shapely.difference(shapely.polygons(bodies),product['base']))
    ids={frame_key(p)[:3]:i for i,p in enumerate(original_poses)}
    request=next(iter(request_keys(product['poses'],[curve['pair']])))
    pair=tuple(ids[key] for key in request)
    positions=np.array([original_poses[i]['position'] for i in pair])
    headings=np.array([original_poses[i]['heading'] for i in pair])
    original_bodies=body_outlines(positions,np.arctan2(headings[:,1],headings[:,0]),vehicle['widthM']*.5,
                                  vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
    return dict(entry=curve['pair'][0],exit=curve['pair'][1],accepted=True,
                path=shapely.get_coordinates(curve['candidate']['path']).tolist(),report=curve['candidate']['report'],
                original_source_path=shapely.get_coordinates(original_routes[pair]).tolist(),
                original_endpoint_bodies=original_bodies.tolist(),
                diagnostic_body_outline=bodies[np.argmax(outside)].tolist())


def real_case(root,name,module,pair,distance,output,sections=None):
    before_path=root/'window-1000-body'/f'{name}-routes.json'
    before=json.loads(before_path.read_text())
    frame_path=root/'boundary-optimized/hermite-routes.json'
    frames=json.loads(frame_path.read_text())
    network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
    cut_path=root/'cut-setbacks'/f'{name}-cut-setbacks.json'
    cuts=json.loads(cut_path.read_text());restore_cut_plan(network,cuts['plan'])
    poses,_=network.poses(module,{})
    if not all(0<=i<len(poses) for i in pair) or pair[0]==pair[1]:
        raise ValueError('selected route must have distinct valid port indices')
    records={r['module']:r for r in before['modules']}
    records.update((r['module'],r) for r in cuts['changed_modules'])
    previous=records[module] if module in records else network.record(module)[0]
    accepted=[(t['entry'],t['exit']) for t in previous['turns'] if t['accepted']]
    previous_keys=request_keys(poses,accepted)
    target=next(iter(request_keys(poses,[pair])))
    assert target not in previous_keys,'selected request already supported'
    original_routes,_=directed_module_routes(network.graph,poses,network.shape(module),network.tree)
    sections={poses[i]['section'] for i in pair} if sections is None else set(sections)
    distance=network.vehicle['minimumRearAxleRadiusM'] if distance is None else distance
    trial,changes,affected=setback_network(network,module,sections,distance)
    receipt_path=root/'inputs'/f'{name}-input.json'
    receipt=json.loads(receipt_path.read_text())['receipt']
    margin=roadbed_lateral_reach(trial.vehicle)+trial.curbs['arc_error_m']
    region=shapely.union_all([trial.shape(m) for m in affected]).envelope.buffer(margin)
    buildings,tiles=building_reservations(receipt,region)
    section_index=shapely.STRtree(np.array(list(trial.sections.values()),dtype=object))
    required={}
    for owner in affected:
        record=records[owner] if owner in records else network.record(owner)[0]
        owner_poses,_=network.poses(owner,{})
        required[owner]=request_keys(owner_poses,[(t['entry'],t['exit']) for t in record['turns'] if t['accepted']])
    cache=dict(contexts={},hits=0,misses=0)
    products={owner:construct_module(network,trial,owner,sections,affected,region,buildings,section_index,required[owner],cache)
              for owner in sorted(affected)}
    precision_path=root/'directed-surfaces'/f'{name}-flat-surfaces.json'
    precision=json.loads(precision_path.read_text())['precision_m']
    frame_sections=set(sections);frame_trials=[];publication=None
    target_present=lambda group:target in request_keys(group[module]['poses'],[c['pair'] for c in group[module]['curves']])
    if not target_present(products):
        for candidate,changed,owners,cost in joint_heading_trials(trial,module,target):
            candidate_affected=affected|owners
            candidate_region=shapely.union_all([candidate.shape(m) for m in candidate_affected]).envelope.buffer(margin)
            if candidate_affected!=set(required):
                buildings,tiles=building_reservations(receipt,candidate_region)
                for owner in candidate_affected-set(required):
                    record=records[owner] if owner in records else network.record(owner)[0]
                    owner_poses,_=network.poses(owner,{})
                    required[owner]=request_keys(owner_poses,[(t['entry'],t['exit']) for t in record['turns'] if t['accepted']])
            note=dict(angular_cost_rad=cost,accepted=False);frame_trials.append(note)
            try:
                group={owner:construct_module(network,candidate,owner,sections|changed,candidate_affected,candidate_region,
                                               buildings,section_index,required[owner],cache) for owner in sorted(candidate_affected)}
                if not target_present(group):
                    note['reason']='selected_request_not_gained';continue
                publication=publish_coupled_roadbeds(group,candidate,precision)
            except AssertionError as error:
                note['reason']=str(error);continue
            trial,products,affected=candidate,group,candidate_affected
            frame_sections|=changed;note['accepted']=True
            break
    mesh,audit,proofs,surfaces=publication if publication is not None else publish_coupled_roadbeds(products,trial,precision)
    output.mkdir(parents=True,exist_ok=True)
    mesh_path=output/'mesh.npz';np.savez_compressed(mesh_path,**mesh)
    product=products[module]
    gained=[c for c in product['curves'] if not request_keys(product['poses'],[c['pair']])<=previous_keys]
    assert target in request_keys(product['poses'],[c['pair'] for c in gained]),'selected request not gained'
    return dict(place=name,module=module,affected=sorted(affected),setback_m=distance,
        cut_plan=serialize_cut_plan(trial,changes),vehicle=trial.vehicle,source_tiles=tiles,
        changed_frame_sections=sorted(frame_sections),joint_heading_trials=frame_trials,
        candidate_reuse={k:cache[k] for k in ('hits','misses')},
        all_requests_preserved=True,external_frames_unchanged=True,all_body_curves_on_native_mesh=True,
        proofs=proofs,published_frames=published_frames(products),published_surfaces=surfaces,physical_mesh_audit=audit,
        physical_mesh=str(mesh_path),physical_mesh_sha256=digest(mesh_path),
        original_surface_wkb=product['base'].wkb.hex(),planned_surface_wkb=product['planned'].wkb.hex(),
        physical_shape_wkb=shapely.union_all([p['core'] for p in products.values()]).wkb.hex(),
        previous_accepted=len(previous_keys),accepted=len(product['curves']),connections=product['connections'],
        unresolved=[r for p in products.values() for r in p['unresolved']],
        added_area_m2=sum(p['planned'].difference(p['base']).area for p in products.values()),
        trials=[diagnostic_trial(c,product,trial.vehicle,poses,original_routes) for c in gained]+product['unresolved'],
        original_sections=[shapely.get_coordinates(network.sections[s]).tolist() for s in sorted(sections)],
        planned_sections=[shapely.get_coordinates(trial.sections[s]).tolist() for s in sorted(sections)],
        shared_cuts_description=f'Shared cuts moved {distance:g} m; {len(proofs)}/{sum(p["connections"] for p in products.values())} routes across {len(products)} modules',
        input_sha256=before['input_sha256']|{str(p.relative_to(root)):digest(p)
                          for p in (before_path,frame_path,cut_path,receipt_path,precision_path)},
        scope='All public-motor requests of the changed junction and its affected corridors, on one jointly triangulated product; no runtime integration.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path('build/experiments/transport'))
    parser.add_argument('--place',default='ZuerichHauptbahnhof')
    parser.add_argument('--module',type=int,default=102)
    parser.add_argument('--pair',type=int,nargs=2,default=[2,4])
    parser.add_argument('--setback',type=float)
    parser.add_argument('--sections',type=int,nargs='+')
    parser.add_argument('--output',type=Path,default=Path('build/experiments/transport/roadbeds/coupled-zuerich-102'))
    args=parser.parse_args();began=time.perf_counter()
    report=real_case(args.root,args.place,args.module,args.pair,args.setback,args.output,args.sections)
    sources=implementation_files(Path(__file__))+[Path(__file__).with_name('geos_triangulation.py').resolve(),
              Path(__file__).with_name('transport_network_recipes.json').resolve()]
    report.update(elapsed_ms=(time.perf_counter()-began)*1000,
        peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*(1 if sys.platform=='darwin' else 1024),
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources})
    (args.output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda obj:obj.tolist())+'\n')
    print(json.dumps({k:report[k] for k in ('place','module','affected','previous_accepted','accepted','connections',
        'all_requests_preserved','all_body_curves_on_native_mesh','added_area_m2','elapsed_ms','peak_rss_bytes')}),flush=True)


if __name__=='__main__':
    main()
