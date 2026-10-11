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

from infrastructure_cut_setback_experiment import prepared_network,digest,implementation_files
from infrastructure_cut_setbacks import restore_cut_plan,setback_network,external_frames_unchanged,serialize_cut_plan
from infrastructure_directed_routes import directed_module_routes
from infrastructure_flat_surfaces import shared_mesh
from infrastructure_junction_surfaces import motor_surface,same_island_winding
from infrastructure_junction_turns import connect_lanes
from infrastructure_route_turns import rounded_source_route
from infrastructure_shared_frames import frame_key
from infrastructure_source_roadbeds import construct_route_roadbed,roadbed_piece,roadbed_body_check
from infrastructure_turn_roadbed_experiment import building_reservations,reservations
from infrastructure_turn_roadbeds import compose_roadbeds,preserves_voids,preserves_coverage,published_roadbed_surface,roadbed_lateral_reach
from infrastructure_vehicle_sweep import body_outlines,turn_sampler


def request_keys(poses,routes):
    keys=[frame_key(p)[:3] for p in poses]
    assert len(set(keys))==len(keys),'ambiguous source contact identity'
    return {(keys[a],keys[b]) for a,b in routes}


def existing_surface_curve(entry,exit,route,surface,vehicle,error):
    path,note=connect_lanes(entry,exit,surface,vehicle,error)
    if path is not None and same_island_winding(path,route,surface):
        poses=dict(entry_pose={k:entry[k].tolist() for k in ('position','heading')},
                   exit_pose={k:exit[k].tolist() for k in ('position','heading')})
        return dict(path=path,pieces=[roadbed_piece(note|poses)],roadbed=shapely.Polygon(),report=note)
    path,note=rounded_source_route(entry,exit,route,surface,vehicle,error,winding_surface=surface)
    if path is not None and same_island_winding(path,route,surface):
        return dict(path=path,pieces=[roadbed_piece(n) for n in note['pieces'][::-1]],
                    roadbed=shapely.Polygon(),report=note)
    return None


def construct_module(network,trial,owner,sections,affected,region,buildings,section_index):
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
    curves=[]
    for pair,route in sorted(routes.items()):
        entry,exit=(poses[i] for i in pair)
        candidate=existing_surface_curve(entry,exit,route,floor,trial.vehicle,trial.curbs['arc_error_m'])
        existing=candidate is not None
        if candidate is None:
            candidate,notes=construct_route_roadbed(entry,exit,floor,reserved,route,
                                                   trial.vehicle,trial.curbs['arc_error_m'])
            assert candidate is not None,(owner,pair,[n.get('reason') for n in notes])
        curves.append(dict(pair=pair,candidate=candidate,existing_surface=existing,source_route=route))
    base,core,planned,physical,quantum=compose_roadbeds(floor,shape,[c['candidate']['roadbed'] for c in curves])
    assert preserves_voids(base,planned) and preserves_voids(core,physical),'changed void topology'
    addition=planned.difference(base)
    assert addition.intersection(reserved).area<=1e-8,'reservation overlap'
    assert not len(section_index.query(addition,predicate='dwithin',distance=1e-7)),'changed shared cut'
    ids=trial.mesh[1].query(addition,predicate='intersects')
    owners=set(map(int,trial.product['face_owners'][ids]))|affected
    assert not any(addition.intersection(trial.shape(m)).area>1e-8 for m in owners if m!=owner),'owner overlap'
    print(json.dumps(dict(module=owner,requests=len(routes),constructed=len(curves),
                          existing_surface=sum(c['existing_surface'] for c in curves),
                          added_area_m2=addition.area)),flush=True)
    return dict(base=base,core=core,planned=planned,physical=physical,quantum=quantum,curves=curves,poses=poses)


def publish_coupled_roadbeds(products,trial,precision):
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
            body=roadbed_body_check(complete,curve['candidate']['pieces'],trial.vehicle,trial.curbs['arc_error_m'])
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
        xy,heading=turn_sampler(piece['profile'])(piece['fit'],np.linspace(0,1,65))[:2]
        outlines.append(body_outlines(xy[0],heading[0],vehicle['widthM']*.5,
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


def real_case(root,name,module,pair,distance,output):
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
    sections={poses[i]['section'] for i in pair}
    distance=network.vehicle['minimumRearAxleRadiusM'] if distance is None else distance
    trial,changes,affected=setback_network(network,module,sections,distance)
    receipt_path=root/'inputs'/f'{name}-input.json'
    receipt=json.loads(receipt_path.read_text())['receipt']
    margin=roadbed_lateral_reach(trial.vehicle)+trial.curbs['arc_error_m']
    region=shapely.union_all([trial.shape(m) for m in affected]).envelope.buffer(margin)
    buildings,tiles=building_reservations(receipt,region)
    section_index=shapely.STRtree(np.array(list(trial.sections.values()),dtype=object))
    products={owner:construct_module(network,trial,owner,sections,affected,region,buildings,section_index)
              for owner in sorted(affected)}
    precision_path=root/'directed-surfaces'/f'{name}-flat-surfaces.json'
    precision=json.loads(precision_path.read_text())['precision_m']
    mesh,audit,proofs,surfaces=publish_coupled_roadbeds(products,trial,precision)
    output.mkdir(parents=True,exist_ok=True)
    mesh_path=output/'mesh.npz';np.savez_compressed(mesh_path,**mesh)
    product=products[module]
    gained=[c for c in product['curves'] if not request_keys(product['poses'],[c['pair']])<=previous_keys]
    assert target in request_keys(product['poses'],[c['pair'] for c in gained]),'selected request not gained'
    return dict(place=name,module=module,affected=sorted(affected),setback_m=distance,
        cut_plan=serialize_cut_plan(trial,changes),vehicle=trial.vehicle,source_tiles=tiles,
        all_requests_preserved=True,external_frames_unchanged=True,all_body_curves_on_native_mesh=True,
        proofs=proofs,published_frames=published_frames(products),published_surfaces=surfaces,physical_mesh_audit=audit,
        physical_mesh=str(mesh_path),physical_mesh_sha256=digest(mesh_path),
        original_surface_wkb=product['base'].wkb.hex(),planned_surface_wkb=product['planned'].wkb.hex(),
        physical_shape_wkb=shapely.union_all([p['core'] for p in products.values()]).wkb.hex(),
        previous_accepted=len(previous_keys),accepted=len(product['curves']),connections=len(product['curves']),
        added_area_m2=sum(p['planned'].difference(p['base']).area for p in products.values()),
        trials=[diagnostic_trial(c,product,trial.vehicle,poses,original_routes) for c in gained],
        original_sections=[shapely.get_coordinates(network.sections[s]).tolist() for s in sorted(sections)],
        planned_sections=[shapely.get_coordinates(trial.sections[s]).tolist() for s in sorted(sections)],
        shared_cuts_description=f'Shared cuts moved {distance:g} m; {len(proofs)}/{len(proofs)} routes across {len(products)} modules',
        input_sha256=before['input_sha256']|{str(p.relative_to(root)):digest(p)
                          for p in (before_path,frame_path,cut_path,receipt_path,precision_path)},
        scope='All public-motor requests of the changed junction and its affected corridors, on one jointly triangulated product; no runtime integration.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path('build/experiments/infrastructure'))
    parser.add_argument('--place',default='ZuerichHauptbahnhof')
    parser.add_argument('--module',type=int,default=102)
    parser.add_argument('--pair',type=int,nargs=2,default=[2,4])
    parser.add_argument('--setback',type=float)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/infrastructure/roadbeds/coupled-zuerich-102'))
    args=parser.parse_args();began=time.perf_counter()
    report=real_case(args.root,args.place,args.module,args.pair,args.setback,args.output)
    sources=implementation_files(Path(__file__))+[Path(__file__).with_name('geos_triangulation.py').resolve(),
              Path(__file__).with_name('infrastructure_network_recipes.json').resolve()]
    report.update(elapsed_ms=(time.perf_counter()-began)*1000,
        peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*(1 if sys.platform=='darwin' else 1024),
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources})
    (args.output/'proof.json').write_text(json.dumps(report,indent=2,default=lambda obj:obj.tolist())+'\n')
    print(json.dumps({k:report[k] for k in ('place','module','affected','previous_accepted','accepted','connections',
        'all_requests_preserved','all_body_curves_on_native_mesh','added_area_m2','elapsed_ms','peak_rss_bytes')}),flush=True)


if __name__=='__main__':
    main()
