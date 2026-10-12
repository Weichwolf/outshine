"""Try jointly shared headings and periodic pose fits on coupled real roadbeds."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,'test/experiments/transport')
import transport_coupled_roadbed_experiment as coupled
from transport_cut_setback_experiment import prepared_network,digest,implementation_files
from transport_cut_setbacks import restore_cut_plan,setback_network,serialize_cut_plan
from transport_directed_routes import directed_module_routes
from transport_shared_frames import frame_key
from transport_turn_roadbed_experiment import building_reservations
from transport_turn_roadbeds import roadbed_lateral_reach


def evaluate(root,name,module,pair,sections,distance,headings,output):
    began=time.perf_counter()
    before_path=root/'window-1000-body'/f'{name}-routes.json'
    frame_path=root/'boundary-optimized/hermite-routes.json'
    cut_path=root/'cut-setbacks'/f'{name}-cut-setbacks.json'
    before=json.loads(before_path.read_text());frames=json.loads(frame_path.read_text())
    network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
    cuts=json.loads(cut_path.read_text());restore_cut_plan(network,cuts['plan'])
    old_poses,_=network.poses(module,{})
    request=next(iter(coupled.request_keys(old_poses,[pair])))
    records={r['module']:r for r in before['modules']}
    records.update((r['module'],r) for r in cuts['changed_modules'])
    trial,changes,affected=setback_network(network,module,set(sections),distance)
    poses,_=trial.poses(module,{})
    by_request={frame_key(p)[:3]:p for p in poses}
    changed_sections=set(sections)
    for key,heading in zip(request,headings):
        pose=by_request[key];full=frame_key(pose)
        trial.frames[full]=dict(position=pose['position'].copy(),heading=np.array(heading))
        changed_sections.add(pose['section']);affected.update(trial.owners[full])
    receipt_path=root/'inputs'/f'{name}-input.json'
    receipt=json.loads(receipt_path.read_text())['receipt']
    region=shapely.union_all([trial.shape(m) for m in affected]).envelope.buffer(roadbed_lateral_reach(trial.vehicle))
    buildings,tiles=building_reservations(receipt,region)
    section_index=shapely.STRtree(np.array(list(trial.sections.values()),dtype=object))
    required={}
    for owner in affected:
        record=records[owner] if owner in records else network.record(owner)[0]
        owner_poses,_=network.poses(owner,{})
        required[owner]=coupled.request_keys(owner_poses,[(t['entry'],t['exit']) for t in record['turns'] if t['accepted']])
    products={owner:coupled.construct_module(network,trial,owner,changed_sections,affected,region,
                                             buildings,section_index,required[owner]) for owner in sorted(affected)}
    precision=json.loads((root/'directed-surfaces'/f'{name}-flat-surfaces.json').read_text())['precision_m']
    mesh,audit,proofs,surfaces=coupled.publish_coupled_roadbeds(products,trial,precision)
    product=products[module]
    gained=[c for c in product['curves'] if not coupled.request_keys(product['poses'],[c['pair']])<=required[module]]
    original_routes,_=directed_module_routes(network.graph,old_poses,network.shape(module),network.tree)
    target_gained=request in coupled.request_keys(product['poses'],[c['pair'] for c in gained])
    result=dict(place=name,module=module,setback_m=distance,target_gained=target_gained,
                previous_accepted=len(required[module]),accepted=len(product['curves']),connections=product['connections'],
                affected=sorted(affected),elapsed_ms=(time.perf_counter()-began)*1000,
                added_area_m2=sum(p['planned'].difference(p['base']).area for p in products.values()))
    if target_gained:
        output.mkdir(parents=True,exist_ok=True)
        mesh_path=output/'mesh.npz';np.savez_compressed(mesh_path,**mesh)
        result.update(vehicle=trial.vehicle,cut_plan=serialize_cut_plan(trial,changes),source_tiles=tiles,
                      frames=coupled.published_frames(products),proofs=proofs,physical_mesh=str(mesh_path),
                      physical_mesh_sha256=digest(mesh_path),all_body_curves_on_native_mesh=True,
                      all_requests_preserved=True,unresolved=[r for p in products.values() for r in p['unresolved']],
                      changed_frame_sections=sorted(changed_sections),
                      original_surface_wkb=product['base'].wkb_hex,planned_surface_wkb=product['planned'].wkb_hex,
                      physical_shape_wkb=shapely.union_all([p['core'] for p in products.values()]).wkb_hex,
                      original_sections=[shapely.get_coordinates(network.sections[s]).tolist() for s in sorted(changed_sections)],
                      planned_sections=[shapely.get_coordinates(trial.sections[s]).tolist() for s in sorted(changed_sections)],
                      shared_cuts_description=f'Cut moved {distance:g} m; shared headings fitted; {len(proofs)}/'+str(sum(p['connections'] for p in products.values()))+' routes',
                      trials=[coupled.diagnostic_trial(c,product,trial.vehicle,old_poses,original_routes) for c in gained],
                      scope='Unintegrated periodic-fit/shared-heading experiment with joint mesh and prior-route proofs.')
        sources=implementation_files(Path(coupled.__file__))
        sources+=[Path(__file__).resolve(),Path(__file__).with_name('angular_branch_probe.py').resolve()]
        result.update(implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},
                      measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip())
        (output/'proof.json').write_text(json.dumps(result,indent=2,default=lambda a:a.tolist())+'\n')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path('build/experiments/transport'))
    parser.add_argument('--choices',type=Path,default=Path('build/experiments/transport/roadbeds/angular-heading-choices/branches.json'))
    parser.add_argument('--output',type=Path,default=Path('build/experiments/transport/roadbeds/joint-heading-probe'))
    parser.add_argument('--maximum-candidates',type=int,default=8)
    args=parser.parse_args();choices=json.loads(args.choices.read_text())
    cases=[(case,row) for case in choices['cases'] for row in case['heading_choices']['candidates']]
    cases.sort(key=lambda values:values[0]['setback_m']+choices['required_radius_m']*values[1]['angular_cost_rad'])
    results=[];args.output.mkdir(parents=True,exist_ok=True)
    for index,(case,row) in enumerate(cases[:args.maximum_candidates]):
        try:
            result=evaluate(args.root,choices['place'],choices['module'],(3,1),[9515],case['setback_m'],
                            row['headings'],args.output/f'candidate-{index}')
            fields=('place','module','setback_m','target_gained','previous_accepted','accepted','connections',
                    'affected','elapsed_ms','added_area_m2')
            result={k:result[k] for k in fields}
        except AssertionError as error:result=dict(reason=str(error),target_gained=False)
        result.update(candidate=index,setback_m=case['setback_m'],angular_cost_rad=row['angular_cost_rad'],headings=row['headings'])
        results.append(result)
        print(json.dumps(result),flush=True)
        (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
        if result['target_gained']:break


if __name__=='__main__':
    main()
