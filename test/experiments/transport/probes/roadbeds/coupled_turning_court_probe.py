"""Publish a turning court with straight approaches and proved neighboring owners."""

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
from transport_cut_setbacks import restore_cut_plan,setback_network,serialize_cut_plan
from transport_directed_routes import directed_module_routes
from transport_fitted_roadbeds import spline_roadbed
from transport_junction_surfaces import motor_surface,same_island_winding
from transport_spline_routes import checked_spline_path
from transport_surface_regions import polygonal_region
from steering_roadbed_candidate_probe import source_buildings,building_cuts
from steering_roadbed_product_probe import seeded_cache


def straight_controls(begin,end):
    return begin+np.linspace(0,1,6)[:,None]*(end-begin)


def approach_controls(note,old,new,pair):
    controls=np.array(note['curve_controls'])
    for index,i in enumerate(pair):
        end=controls[0,0] if index==0 else controls[-1,-1]
        assert np.allclose(old[i]['position'],end,rtol=0,atol=1e-7)
        assert np.allclose(old[i]['heading'],new[i]['heading'],rtol=0,atol=1e-8)
    prefix=straight_controls(new[pair[0]]['position'],controls[0,0])
    suffix=straight_controls(controls[-1,-1],new[pair[1]]['position'])
    return np.concatenate([prefix[None],controls,suffix[None]])


def seeds_for(cases,old,poses,routes,floor,reserved,vehicle,error):
    seeds={}
    for pair,case in cases.items():
        report=json.loads((case/'results.json').read_text())
        assert report['cases'][0]['accepted']
        for name,value in report['implementation_sha256'].items():assert coupled.digest(Path(name))==value
        note=json.loads((case/'0-candidate.json').read_text())
        geometry=json.loads((case/'0-geometry.json').read_text())
        original=shapely.from_wkb(geometry['original'])
        court=shapely.from_wkb(geometry['planned']).difference(original)
        controls=approach_controls(note,old,poses,pair)
        support,audit=spline_roadbed(controls,vehicle,error);assert support is not None,audit
        support=support.union(court).difference(reserved)
        support=polygonal_region(support)
        planned=floor.union(support)
        path,proof=checked_spline_path(controls,planned,vehicle,error);assert path is not None,proof
        assert same_island_winding(path,routes[pair],floor)
        piece=dict(profile='G2-quintic-spline',fit=dict(controls=controls,
            maximum_curvature_per_m=np.array([proof['curvature_bound_per_m']])),
            begin=poses[pair[0]]['position'],end=poses[pair[1]]['position'])
        seeds[pair]=dict(path=path,pieces=[piece],roadbed=support,surface=planned,
                        report=dict(proof,construction=audit,recipe='turning-court-with-straight-approaches'))
    return seeds


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--place',required=True)
    parser.add_argument('--module',type=int,required=True)
    parser.add_argument('--pair',type=int,nargs=2,action='append',required=True)
    parser.add_argument('--case',type=Path,action='append',required=True)
    parser.add_argument('--setback',type=float,default=None)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();begin=time.perf_counter()
    if len(args.pair)!=len(args.case) or len({tuple(p) for p in args.pair})!=len(args.pair):parser.error('one case per distinct pair required')
    root=Path('build/experiments/transport');name,module=args.place,args.module
    before_path=root/'window-1000-body'/f'{name}-routes.json';before=json.loads(before_path.read_text())
    frame_path=root/'boundary-optimized/hermite-routes.json';frames=json.loads(frame_path.read_text())
    network=coupled.prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
    cut_path=root/'cut-setbacks'/f'{name}-cut-setbacks.json';cuts=json.loads(cut_path.read_text());restore_cut_plan(network,cuts['plan'])
    old,_=network.poses(module,{});sections={old[i]['section'] for pair in args.pair for i in pair}
    original_routes,_=directed_module_routes(network.graph,old,network.shape(module),network.tree)
    distance=network.vehicle['minimumRearAxleRadiusM'] if args.setback is None else args.setback
    trial,changes,affected=setback_network(network,module,sections,distance)
    poses,_=trial.poses(module,{})
    routes,_=directed_module_routes(trial.graph,poses,trial.shape(module),trial.tree)
    records={r['module']:r for r in before['modules']};records.update((r['module'],r) for r in cuts['changed_modules'])
    required={}
    for owner in affected:
        record=records[owner] if owner in records else network.record(owner)[0]
        owner_poses,_=network.poses(owner,{})
        required[owner]=coupled.request_keys(owner_poses,[(t['entry'],t['exit']) for t in record['turns'] if t['accepted']])
    margin=max(trial.vehicle['frontFromRearAxleM'],trial.vehicle['rearOverhangM'])+trial.vehicle['widthM']*.5+2*trial.curbs['arc_error_m']
    region=shapely.union_all([trial.shape(o) for o in affected]).envelope.buffer(margin)
    floor=motor_surface(shapely.union_all(list(routes.values())).envelope,trial.graph,trial.tree,trial.mesh,
                        trial.vehicle,trial.curbs['corner_radius_m'],trial.curbs['arc_error_m'])
    receipt_path=root/'inputs'/f'{name}-input.json';receipt=json.loads(receipt_path.read_text())['receipt']
    sources=source_buildings(receipt,region);other=coupled.reservations(trial,region).difference(floor)
    cases={tuple(p):c for p,c in zip(args.pair,args.case)}
    seeds=seeds_for(cases,old,poses,routes,floor,other,trial.vehicle,trial.curbs['arc_error_m'])
    buildings,building_audit=building_cuts(sources,floor,shapely.union_all([s['surface'] for s in seeds.values()]))
    protected=shapely.union_all([b['adjusted'] for b in buildings]);reserved=protected.union(other)
    seeds=seeds_for(cases,old,poses,routes,floor,reserved,trial.vehicle,trial.curbs['arc_error_m'])
    cache=seeded_cache(floor,reserved,routes,poses,seeds,trial.vehicle,trial.curbs['arc_error_m'],required[module])
    section_index=shapely.STRtree(np.array(list(trial.sections.values()),dtype=object))
    products={}
    for owner in sorted(affected):
        products[owner]=construct_complete_module(network,trial,owner,sections,affected,region,protected,
            section_index,required[owner],cache,reserved)
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
        original_surface_wkb=product['base'].wkb.hex(),planned_surface_wkb=product['planned'].wkb.hex(),
        original_physical_shape_wkb=shapely.union_all([network.shape(o) for o in affected]).wkb.hex(),
        physical_shape_wkb=shapely.union_all([p['physical'] for p in products.values()]).wkb.hex(),
        previous_accepted=len(required[module]),accepted=len(product['curves']),connections=product['connections'],
        unresolved=[r for p in products.values() for r in p['unresolved']],
        trials=[coupled.diagnostic_trial(c,product,trial.vehicle,old,original_routes) for c in product['curves']],
        added_area_m2=sum(p['planned'].difference(p['base']).area for p in products.values()),
        original_sections=[shapely.get_coordinates(network.sections[s]).tolist() for s in sorted(sections)],
        planned_sections=[shapely.get_coordinates(trial.sections[s]).tolist() for s in sorted(sections)],
        shared_cuts_description=f'Cuts moved {distance:g} m; {len(proofs)} proved routes across {len(affected)} owners; Buildings runtime pending',
        elapsed_ms=(time.perf_counter()-begin)*1000,building_runtime_integration_pending=True,
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        scope='All public motor requests across the court and affected neighbors on one mesh; Buildings runtime and full-window integration remain open.')
    files=[before_path,frame_path,cut_path,receipt_path,precision_path,*[c/f for c in args.case for f in ('results.json','0-candidate.json','0-geometry.json')]]
    saved['input_sha256']=before['input_sha256']|{str(p.resolve().relative_to(root.resolve())):coupled.digest(p) for p in files}
    code=Path(__file__).resolve().parents[2]
    implementations=coupled.implementation_files(code/'transport_coupled_roadbed_experiment.py')
    implementations+=coupled.implementation_files(code/'transport_complete_roadbed_module.py')
    implementations.extend([Path(__file__).resolve(),Path(source_buildings.__code__.co_filename),Path(seeded_cache.__code__.co_filename)])
    saved['implementation_sha256']={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in set(implementations)}
    (args.output/'proof.json').write_text(json.dumps(saved,indent=2,default=lambda obj:obj.tolist() if hasattr(obj,'tolist') else obj.wkb.hex())+'\n')
    print(json.dumps({k:saved[k] for k in ('accepted','connections','affected','added_area_m2','elapsed_ms')}))


if __name__=='__main__':main()
