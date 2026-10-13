"""Publish turning courts with G2 approaches and proved local neighboring owners."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_coupled_roadbed_experiment as coupled
from transport_complete_roadbed_module import construct_complete_module
from transport_cut_setbacks import restore_cut_plan,setback_network,serialize_cut_plan,fork_cut_plan
from transport_directed_routes import directed_module_routes
from transport_fitted_roadbeds import spline_roadbed
from transport_junction_surfaces import motor_surface,same_island_winding
from transport_spline_routes import checked_spline_path
from transport_surface_regions import polygonal_region
from transport_court_partition import partition_court
from transport_shared_frames import frame_key
from transport_joined_cut_setbacks import setback_joined_network
from steering_roadbed_candidate_probe import source_buildings,building_cuts
from steering_roadbed_product_probe import seeded_cache,declared_public_crossing


def approach_segment(begin,end):
    distance=np.linalg.norm(end['position']-begin['position'])
    a,b=begin['position'],end['position'];u,v=begin['heading'],end['heading']
    return np.array([a,a+u*distance/5,a+2*u*distance/5,
                     b-2*v*distance/5,b-v*distance/5,b])


def approach_controls(note,old,new,pair):
    controls=np.array(note['curve_controls'])
    endpoints=controls[[0,-1],[0,-1]]
    current=np.array([new[i]['position'] for i in pair])
    direct=np.allclose(endpoints,current,rtol=0,atol=1e-7)
    for index,i in enumerate(pair):
        end=controls[0,0] if index==0 else controls[-1,-1]
        pose=new[i] if direct else old[i]
        assert np.allclose(pose['position'],end,rtol=0,atol=1e-7),'court curve endpoint differs from source frame'
        tangent=controls[0,1]-controls[0,0] if index==0 else controls[-1,-1]-controls[-1,-2]
        assert np.linalg.norm(tangent)>0 and np.allclose(tangent/np.linalg.norm(tangent),pose['heading'],rtol=0,atol=1e-8), 'court curve tangent differs from source frame'
    if direct:return controls
    segments=[controls]
    if np.linalg.norm(new[pair[0]]['position']-controls[0,0])>1e-7:
        segments.insert(0,approach_segment(new[pair[0]],old[pair[0]])[None])
    if np.linalg.norm(controls[-1,-1]-new[pair[1]]['position'])>1e-7:
        segments.append(approach_segment(old[pair[1]],new[pair[1]])[None])
    return np.concatenate(segments)


def selected_court_files(case):
    explicit=case.is_file()
    directory=case.parent if explicit else case
    report=json.loads((directory/'results.json').read_text())
    accepted=[(i,r) for i,r in enumerate(report['cases']) if r['accepted']]
    assert accepted,'no proved court candidate'
    if explicit:
        index=int(case.name.removesuffix('-candidate.json'))
        assert case.name==f'{index}-candidate.json' and index in dict(accepted),'selected candidate was not accepted'
    else:index,_=min(accepted,key=lambda p:(p[1]['added_area_m2'],p[1]['length_m'],p[0]))
    for name,value in report['implementation_sha256'].items():assert coupled.digest(Path(name))==value
    return directory/f'{index}-candidate.json',directory/f'{index}-geometry.json'


def seeds_for(cases,old,poses,routes,floor,reserved,vehicle,error):
    seeds={}
    current={frame_key(p)[:3]:i for i,p in enumerate(poses)}
    assert len(current)==len(poses),'ambiguous source contact'
    for original_pair,case in cases.items():
        pair=tuple(current[frame_key(old[i])[:3]] for i in original_pair)
        candidate_path,geometry_path=selected_court_files(case)
        note=json.loads(candidate_path.read_text())
        geometry=json.loads(geometry_path.read_text())
        original=shapely.from_wkb(geometry['original'])
        court=shapely.from_wkb(geometry['planned']).difference(original)
        controls=approach_controls(note,[old[i] for i in original_pair],[poses[i] for i in pair],(0,1))
        support,audit=spline_roadbed(controls,vehicle,error);assert support is not None,audit
        support=support.union(court).difference(reserved)
        support=polygonal_region(support)
        planned=polygonal_region(floor.union(support))
        path,proof=checked_spline_path(controls,planned,vehicle,error);assert path is not None,proof
        assert same_island_winding(path,routes[pair],floor)
        piece=dict(profile='G2-quintic-spline',fit=dict(controls=controls,
            maximum_curvature_per_m=np.array([proof['curvature_bound_per_m']])),
            begin=poses[pair[0]]['position'],end=poses[pair[1]]['position'])
        seeds[pair]=dict(path=path,pieces=[piece],roadbed=support,surface=planned,
                        report=dict(proof,construction=audit,recipe='turning-court-with-straight-approaches'))
    return seeds


def local_court_sections(network,module,poses,cases):
    scope={poses[i]['section'] for pair in cases for i in pair}
    motor={p['section'] for p in poses};additions=[]
    for case in cases.values():
        _,path=selected_court_files(case);geometry=json.loads(path.read_text())
        additions.append(shapely.from_wkb(geometry['planned']).difference(shapely.from_wkb(geometry['original'])))
    addition=shapely.union_all(additions)
    scope.update(p['section'] for p in network.junctions[module] if p['section'] in motor
                 and p['line'].distance(addition)<=2*network.curbs['arc_error_m'])
    return scope


def required_court_seeds(trial,poses,routes,seeds,floor,reserved,required):
    planned=polygonal_region(shapely.union_all([floor,*[s['surface'] for s in seeds.values()]]))
    cache=seeded_cache(planned,reserved,routes,poses,seeds,trial.vehicle,trial.curbs['arc_error_m'],required)
    complete=dict(seeds)
    for pair,route in sorted(routes.items()):
        if pair in seeds or not coupled.request_keys(poses,[pair])<=required:continue
        candidate,existing,_=coupled.roadbed_candidate(poses[pair[0]],poses[pair[1]],route,planned,reserved,
                            trial.vehicle,trial.curbs['arc_error_m'],True,cache)
        if candidate is not None:
            complete[pair]=dict(candidate,existing_surface=existing,
                                surface=polygonal_region(planned.union(candidate['roadbed'])))
    return complete


def grow_court_ports(trial,module,affected,poses,seeds,floor,scope):
    public_spaces={}
    ports=[]
    for port in trial.junctions[module]:
        if port['section'] not in scope:continue
        p=next((p for p in poses if p['section']==port['section']),None)
        if p is None:continue
        ports.append(dict(section=port['section'],owner=port['corridor'],line=port['line'],
                          away=p['heading']*(-1 if p['incoming'] else 1)))
    addition=polygonal_region(shapely.union_all([s['surface'] for s in seeds.values()]).difference(floor))
    parts,lines,partition_audit=partition_court({o:trial.shape(o) for o in affected},module,addition,ports)
    trial.shapes.update(parts);sections=set(lines);changes=[]
    for port in trial.junctions[module]:
        if port['section'] not in lines:continue
        p=next(p for p in poses if p['section']==port['section']);s=port['section']
        trial.sections[s]=lines[s];port['line']=lines[s]
        changes.append(dict(junction=module,section=s,corridor=port['corridor'],edge=p['edge'],station=p['station'],
                            shared_section=shapely.get_coordinates(lines[s]).tolist(),recipe='expanded-fixed-plane-port'))
    trial.registry.pop(module,None)
    current,unused=trial.poses(module,{})
    previous={frame_key(p)[:3]:p for p in poses}
    assert {frame_key(p)[:3] for p in current}==previous.keys(),dict(
        reason='port growth changes source contacts',old=sorted(previous),new=sorted(frame_key(p)[:3] for p in current),unused=unused)
    for p in current:
        saved_pose=previous[frame_key(p)[:3]]
        assert abs(p['station']-saved_pose['station'])<1e-7,'port growth moves source stations'
        trial.frames[frame_key(p)]={k:saved_pose[k] for k in ('position','heading')}
    poses,_=trial.poses(module,{})
    routes,_=directed_module_routes(trial.graph,poses,trial.shape(module),trial.tree)
    for owner in affected:
        owner_poses,_=trial.poses(owner,{})
        owner_routes,_=directed_module_routes(trial.graph,owner_poses,trial.shape(owner),trial.tree)
        demand=shapely.union_all(list(owner_routes.values())).envelope if owner_routes else trial.shape(owner)
        source=motor_surface(demand,trial.graph,trial.tree,trial.mesh,trial.vehicle,
                             trial.curbs['corner_radius_m'],trial.curbs['arc_error_m'])
        public_spaces[owner]=polygonal_region(source.union(addition))
    return poses,routes,sections,changes,public_spaces,addition,partition_audit


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--place',required=True)
    parser.add_argument('--module',type=int,required=True)
    parser.add_argument('--pair',type=int,nargs=2,action='append',required=True)
    parser.add_argument('--case',type=Path,action='append',required=True)
    parser.add_argument('--setback',type=float,default=None)
    parser.add_argument('--grow-ports',action='store_true')
    parser.add_argument('--joined-cuts',action='store_true')
    parser.add_argument('--cut-sections',type=int,nargs='+')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();begin=time.perf_counter()
    if len(args.pair)!=len(args.case) or len({tuple(p) for p in args.pair})!=len(args.pair):parser.error('one case per distinct pair required')
    root=Path('build/experiments/transport');name,module=args.place,args.module
    before_path=root/'window-1000-body'/f'{name}-routes.json';before=json.loads(before_path.read_text())
    frame_path=root/'boundary-optimized/hermite-routes.json';frames=json.loads(frame_path.read_text())
    network=coupled.prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
    cut_path=root/'cut-setbacks'/f'{name}-cut-setbacks.json';cuts=json.loads(cut_path.read_text());restore_cut_plan(network,cuts['plan'])
    old,_=network.poses(module,{});sections={old[i]['section'] for pair in args.pair for i in pair}
    if args.cut_sections is not None:
        if not set(args.cut_sections)<=sections:parser.error('moved sections must belong to the selected source requests')
        sections=set(args.cut_sections)
    original_routes,_=directed_module_routes(network.graph,old,network.shape(module),network.tree)
    distance=network.vehicle['minimumRearAxleRadiusM'] if args.setback is None else args.setback
    scope=local_court_sections(network,module,old,dict(zip(map(tuple,args.pair),args.case))) if args.grow_ports or distance==0 else set()
    if distance==0:
        affected={module}|{p['corridor'] for p in network.junctions[module] if p['section'] in scope}
        trial=fork_cut_plan(network,module,set());changes=[];sections=set()
    else:
        make_cuts=setback_joined_network if args.joined_cuts else setback_network
        trial,changes,affected=make_cuts(network,module,sections,distance)
    if args.grow_ports:
        affected.update(p['corridor'] for p in trial.junctions[module] if p['section'] in scope)
    poses,_=trial.poses(module,{})
    routes,_=directed_module_routes(trial.graph,poses,trial.shape(module),trial.tree)
    records={r['module']:r for r in before['modules']};records.update((r['module'],r) for r in cuts['changed_modules'])
    margin=max(trial.vehicle['frontFromRearAxleM'],trial.vehicle['rearOverhangM'])+trial.vehicle['widthM']*.5+2*trial.curbs['arc_error_m']
    region=shapely.union_all([trial.shape(o) for o in affected]).envelope.buffer(margin)
    floor=motor_surface(shapely.union_all(list(routes.values())).envelope,trial.graph,trial.tree,trial.mesh,
                        trial.vehicle,trial.curbs['corner_radius_m'],trial.curbs['arc_error_m'])
    original_floor=floor
    cases={tuple(p):c for p,c in zip(args.pair,args.case)}
    crossings=[shapely.from_wkb(json.loads(selected_court_files(case)[1].read_text())['crossing']) for case in cases.values()]
    crossing=polygonal_region(shapely.union_all(crossings))
    shared_crossing,crossing_owners=declared_public_crossing(trial,region,crossing)
    affected.update(crossing_owners)
    region=shapely.union_all([trial.shape(o) for o in affected]).envelope.buffer(margin)
    floor=floor.union(shared_crossing)
    required={}
    for owner in affected:
        record=records[owner] if owner in records else network.record(owner)[0]
        owner_poses,_=network.poses(owner,{})
        required[owner]=coupled.request_keys(owner_poses,[(t['entry'],t['exit']) for t in record['turns'] if t['accepted']])
    receipt_path=root/'inputs'/f'{name}-input.json';receipt=json.loads(receipt_path.read_text())['receipt']
    sources=source_buildings(receipt,region);other=coupled.reservations(trial,region).difference(floor.union(crossing))
    seeds=seeds_for(cases,old,poses,routes,floor,other,trial.vehicle,trial.curbs['arc_error_m'])
    buildings,building_audit=building_cuts(sources,floor,shapely.union_all([s['surface'] for s in seeds.values()]))
    protected=shapely.union_all([b['adjusted'] for b in buildings]);reserved=protected.union(other)
    seeds=seeds_for(cases,old,poses,routes,floor,reserved,trial.vehicle,trial.curbs['arc_error_m'])
    public_spaces={};addition=shared_crossing;partition_audit={}
    if not shared_crossing.is_empty:
        for owner in affected:
            source=motor_surface(trial.shape(owner),trial.graph,trial.tree,trial.mesh,trial.vehicle,
                                 trial.curbs['corner_radius_m'],trial.curbs['arc_error_m'])
            public_spaces[owner]=source.union(shared_crossing)
        public_spaces[module]=floor
    if args.grow_ports:
        growth_seeds=required_court_seeds(trial,poses,routes,seeds,floor,reserved,required[module])
        poses,routes,grown_sections,growth_changes,public_spaces,addition,partition_audit=grow_court_ports(
            trial,module,affected,poses,growth_seeds,floor,scope)
        sections|=grown_sections;changes+=growth_changes
        public_spaces={o:g.union(shared_crossing) for o,g in public_spaces.items()}
        addition=polygonal_region(addition.union(shared_crossing))
        floor=public_spaces[module]
        reserved=protected.union(coupled.reservations(trial,region).difference(floor.union(crossing)))
        seeds=seeds_for(cases,old,poses,routes,floor,reserved,trial.vehicle,trial.curbs['arc_error_m'])
        seeds.update((pair,candidate) for pair,candidate in growth_seeds.items() if pair not in seeds)
    cache=seeded_cache(floor,reserved,routes,poses,seeds,trial.vehicle,trial.curbs['arc_error_m'],required[module])
    section_index=shapely.STRtree(np.array(list(trial.sections.values()),dtype=object))
    products={}
    for owner in sorted(affected):
        products[owner]=construct_complete_module(network,trial,owner,sections,affected,region,protected,
            section_index,required[owner],cache,reserved,public_floor=public_spaces.get(owner))
    product=products[module];assert seeds.keys()<={c['pair'] for c in product['curves']},product['unresolved']
    precision_path=root/'directed-surfaces'/f'{name}-flat-surfaces.json'
    precision=json.loads(precision_path.read_text())['precision_m']
    mesh,audit,proofs,surfaces=coupled.publish_coupled_roadbeds(products,trial,precision)
    args.output.mkdir(parents=True,exist_ok=True);mesh_path=args.output/'mesh.npz';np.savez_compressed(mesh_path,**mesh)
    saved=dict(place=name,module=module,affected=sorted(affected),setback_m=distance,cut_plan=serialize_cut_plan(trial,changes),
        changed_frame_sections=sorted(sections),vehicle=trial.vehicle,all_requests_preserved=True,
        external_frames_unchanged=True,all_body_curves_on_native_mesh=True,proofs=proofs,
        published_frames=coupled.published_frames(products),published_surfaces=surfaces,physical_mesh_audit=audit,
        physical_mesh=str(mesh_path),physical_mesh_sha256=coupled.digest(mesh_path),buildings=buildings,building_audit=building_audit,
        shared_public_addition_wkb=addition.wkb.hex(),court_partition=partition_audit,
        original_surface_wkb=original_floor.wkb.hex(),planned_surface_wkb=product['planned'].wkb.hex(),
        original_physical_shape_wkb=shapely.union_all([network.shape(o) for o in affected]).wkb.hex(),
        physical_shape_wkb=shapely.union_all([p['physical'] for p in products.values()]).wkb.hex(),
        previous_accepted=len(required[module]),accepted=len(product['curves']),connections=product['connections'],
        unresolved=[r for p in products.values() for r in p['unresolved']],
        trials=[coupled.diagnostic_trial(c,product,trial.vehicle,old,original_routes) for c in product['curves']],
        added_area_m2=shapely.union_all([p['physical'] for p in products.values()]).difference(
            shapely.union_all([network.shape(o) for o in affected])).area,
        original_sections=[shapely.get_coordinates(network.sections[s]).tolist() for s in sorted(sections)],
        planned_sections=[shapely.get_coordinates(trial.sections[s]).tolist() for s in sorted(sections)],
        shared_cuts_description=f'{f"Widths solved after {distance:g} m cut setback" if args.grow_ports else f"Cuts moved {distance:g} m"}; {len(proofs)} proved routes across {len(affected)} owners; Buildings runtime pending',
        elapsed_ms=(time.perf_counter()-begin)*1000,building_runtime_integration_pending=True,
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        scope='All public motor requests across the court and affected neighbors on one mesh; Buildings runtime and full-window integration remain open.')
    files=[before_path,frame_path,cut_path,receipt_path,precision_path,
           *[p for c in args.case for p in ((c.parent if c.is_file() else c)/'results.json',*selected_court_files(c))]]
    saved['input_sha256']=before['input_sha256']|{str(p.resolve().relative_to(root.resolve())):coupled.digest(p) for p in files}
    code=Path(__file__).resolve().parents[2]
    implementations=coupled.implementation_files(code/'transport_coupled_roadbed_experiment.py')
    implementations+=coupled.implementation_files(code/'transport_complete_roadbed_module.py')
    implementations+=coupled.implementation_files(code/'transport_court_partition.py')
    implementations+=coupled.implementation_files(code/'transport_coupled_roadbed_products.py')
    implementations.extend([Path(__file__).resolve(),Path(source_buildings.__code__.co_filename),Path(seeded_cache.__code__.co_filename)])
    saved['implementation_sha256']={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in set(implementations)}
    (args.output/'proof.json').write_text(json.dumps(saved,indent=2,default=lambda obj:obj.tolist() if hasattr(obj,'tolist') else obj.wkb.hex())+'\n')
    print(json.dumps({k:saved[k] for k in ('accepted','connections','affected','added_area_m2','elapsed_ms')}))


if __name__=='__main__':main()
