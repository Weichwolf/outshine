"""Construct and publish roadbeds across frozen complete public-motor windows."""

import argparse
from collections import Counter
import json
from pathlib import Path
import resource
import subprocess
import sys
import time

import numpy as np
import shapely

from transport_cut_setback_experiment import prepared_network,digest,implementation_files
from transport_cut_setbacks import restore_cut_plan
from transport_coupled_roadbed_products import apply_coupled_product
from transport_coupled_product_checks import verify_request_ledger
from transport_directed_routes import directed_module_routes
from transport_flat_surfaces import shared_mesh
from transport_junction_surfaces import motor_surface
from transport_route_connectivity import network_connectivity
from transport_source_roadbeds import construct_route_roadbed,roadbed_body_check
from transport_travel import MODES
from transport_turn_roadbed_experiment import building_reservations,verify_small
from transport_turn_roadbeds import compose_roadbeds,preserves_voids,preserves_coverage,published_roadbed_surface,roadbed_lateral_reach


def trial_key(row):
    return row['module'],row['entry'],row['exit']


def propose_roadbeds(network,current,reserved,changed):
    sections=shapely.STRtree(np.array(list(network.sections.values()),dtype=object))
    proposed,trials=[],[]
    for record in current['modules']:
        if record['accepted']==record['connections']:
            continue
        module=record['module'];shape=network.shape(module);poses,_=network.poses(module,{})
        floor=motor_surface(shape,network.graph,network.tree,network.mesh,network.vehicle,
                            network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
        routes,_=directed_module_routes(network.graph,poses,shape,network.tree)
        for turn in record['turns']:
            if turn['accepted']:
                continue
            pair=turn['entry'],turn['exit'];began=time.perf_counter()
            candidate,notes=construct_route_roadbed(poses[pair[0]],poses[pair[1]],floor,reserved,
                                routes[pair],network.vehicle,network.curbs['arc_error_m'])
            note=dict(module=module,entry=pair[0],exit=pair[1],accepted=False,
                      elapsed_ms=(time.perf_counter()-began)*1000,
                      reasons=dict(Counter(r['reason'] for r in notes)))
            trials.append(note)
            if candidate is None:
                note['reason']='no_constructive_candidate'
                continue
            note['profile']=candidate['report']['profile']
            addition=candidate['addition']
            ids=network.mesh[1].query(addition,predicate='intersects')
            owners=set(map(int,network.product['face_owners'][ids]))|changed
            if any(addition.intersection(network.shape(owner)).area>1e-8 for owner in owners if owner!=module):
                note['reason']='another_physical_owner'
                continue
            if len(sections.query(addition,predicate='dwithin',distance=1e-7)):
                note['reason']='touches_shared_section'
                continue
            note['reason']='pending_combined_native_mesh'
            proposed.append(dict(module=module,pair=pair,candidate=candidate,note=note,shape=shape,floor=floor))
    return proposed,trials


def select_roadbeds(proposed,required):
    additions=np.array([p['candidate']['addition'] for p in proposed],dtype=object)
    index=shapely.STRtree(additions)
    selected=set()
    order=sorted(range(len(proposed)),key=lambda i:(trial_key(proposed[i]['note']) not in required,
                            additions[i].area,proposed[i]['module'],proposed[i]['pair']))
    for i in order:
        conflicts=[int(j) for j in index.query(additions[i],predicate='intersects')
                   if int(j) in selected and proposed[int(j)]['module']!=proposed[i]['module']
                   and additions[i].intersection(additions[int(j)]).area>1e-8]
        if conflicts:
            proposed[i]['note']['reason']='conflicts_with_selected_addition'
        else:
            selected.add(i)
    return [proposed[i] for i in sorted(selected)]


def publish_roadbeds(choices,network,precision):
    floor,shape=choices[0]['floor'],choices[0]['shape']
    base,core,planned,physical,quantum=compose_roadbeds(floor,shape,[p['candidate']['roadbed'] for p in choices])
    assert preserves_voids(floor,planned) and preserves_voids(shape,physical),'changed_void_topology'
    parts=shapely.get_parts(physical)
    mesh,audit=shared_mesh(physical,parts,np.ones(len(parts),dtype=int),precision)
    complete,_=published_roadbed_surface(mesh,base,core,planned,quantum)
    assert preserves_coverage(floor,complete),'lost_original_public_floor'
    for p in choices:
        body=roadbed_body_check(complete,p['candidate']['pieces'],network.vehicle,network.curbs['arc_error_m'])
        p['note']['published_body']=body
        assert body['fits_surface'],'published_mesh_does_not_support_body'
    return mesh,audit


def solve_window(root,name,frames,required,output,coupled):
    began=time.perf_counter()
    before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
    cut_path=root/'cut-setbacks'/f'{name}-cut-setbacks.json'
    cuts=json.loads(cut_path.read_text())
    network=prepared_network(root,before,frames);restore_cut_plan(network,cuts['plan'])
    records={r['module']:r for r in before['modules']}
    records.update((r['module'],r) for r in cuts['changed_modules'])
    current=dict(before,modules=[records[r['module']] for r in before['modules']])
    before_graph=network_connectivity(network,current)
    baseline_accepted=sum(r['accepted'] for r in current['modules'])
    changed={r['module'] for r in cuts['plan']['parts']}
    occupied=set();products=[]
    for path in coupled:
        if json.loads(path.read_text())['place']!=name:continue
        network,records,required,note=apply_coupled_product(network,records,required,root,path,occupied)
        occupied.update(note['modules']);changed.update(note['modules']);products.append(note)
    current=dict(before,modules=[records[r['module']] for r in before['modules']])
    bounds=shapely.MultiPoint(network.product['vertices']).envelope.buffer(
        roadbed_lateral_reach(network.vehicle)+network.curbs['arc_error_m'])
    receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
    buildings,tiles=building_reservations(receipt,bounds)
    nonmotor=~np.any(network.graph['travel_public']&MODES['motor']!=0,axis=1)
    other=shapely.union_all(shapely.buffer(network.graph['lines'][nonmotor],network.graph['half_widths'][nonmotor],quad_segs=2))
    proposed,trials=propose_roadbeds(network,current,buildings.union(other),changed)
    selected=select_roadbeds(proposed,required)
    precision=json.loads((root/'directed-surfaces'/f'{name}-flat-surfaces.json').read_text())['precision_m']
    for module in sorted({p['module'] for p in selected}):
        choices=[p for p in selected if p['module']==module]
        try:
            mesh,audit=publish_roadbeds(choices,network,precision)
        except (AssertionError,ValueError) as error:
            retained=[p for p in choices if trial_key(p['note']) in required]
            for p in choices:p['note'].update(reason='combined_native_rejection',detail=str(error))
            if not retained:
                continue
            choices=retained
            mesh,audit=publish_roadbeds(choices,network,precision)
        path=output/f'{name}-{module}-mesh.npz'
        np.savez_compressed(path,vertices=mesh['vertices'],indices=mesh['indices'])
        for p in choices:
            p['note'].update(accepted=True,reason='combined_native_mesh_checked',
                added_area_m2=float(p['candidate']['addition'].area),mesh=str(path),mesh_sha256=digest(path),
                path=shapely.get_coordinates(p['candidate']['path']).tolist(),mesh_audit=audit)
        proved={p['pair'] for p in choices};original=records[module]
        records[module]=dict(original,accepted=original['accepted']+len(proved),
                      turns=[dict(t,accepted=True) if (t['entry'],t['exit']) in proved else t for t in original['turns']])
    after=dict(current,modules=[records[r['module']] for r in current['modules']])
    accepted={trial_key(t) for t in trials if t['accepted']}
    existing={(r['module'],t['entry'],t['exit']) for r in current['modules'] for t in r['turns'] if t['accepted']}
    assert required<=accepted|existing,f'previously proved roadbeds lost: {sorted(required-(accepted|existing))}'
    after_graph=network_connectivity(network,after)
    (output/f'{name}-connectivity.json').write_text(json.dumps(after_graph,indent=2)+'\n')
    return dict(place=name,trials=trials,proposed=len(proposed),native_accepted=len(accepted),
         before=baseline_accepted,after=sum(r['accepted'] for r in after['modules']),coupled_products=products,
         connections=sum(r['connections'] for r in after['modules']),previous_roadbeds_preserved=len(required),
         weak_components=[before_graph['proved_weak_components'],after_graph['proved_weak_components']],
         requested_weak_components=after_graph['requested_weak_components'],
         strong_components=[before_graph['proved_strong_components'],after_graph['proved_strong_components']],
         profiles=dict(Counter(t['profile'] for t in trials if t['accepted'])),
         reasons=dict(Counter(t['reason'] for t in trials if not t['accepted'])),
         elapsed_ms=(time.perf_counter()-began)*1000,source_tiles=tiles,
         input_sha256=before['input_sha256']|{str(cut_path.relative_to(root)):digest(cut_path)})


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path('build/experiments/transport'))
    parser.add_argument('--reference',type=Path,default=Path('build/experiments/transport/roadbeds/window-probe-results.json'))
    parser.add_argument('--retain',type=Path,nargs='*',default=[])
    parser.add_argument('--coupled',type=Path,nargs='*',default=[])
    parser.add_argument('--output',type=Path,default=Path('build/experiments/transport/roadbeds/windows'))
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    references=json.loads(args.reference.read_text())
    if isinstance(references,dict):references=references['places']
    for report in references:
        for name,value in report['input_sha256'].items():assert digest(args.root/name)==value
    required={r['place']:{trial_key(t) for t in r['trials'] if t['accepted']} for r in references}
    for path in args.retain:
        report=json.loads(path.read_text())
        for name,value in report['input_sha256'].items():assert digest(args.root/name)==value
        required[report['place']].update((report['module'],t['entry'],t['exit']) for t in report['trials'] if t['accepted'])
    frame_path=args.root/'boundary-optimized/hermite-routes.json'
    frames=json.loads(frame_path.read_text())
    checks=verify_small(json.loads(Path(__file__).with_name('transport_network_recipes.json').read_text())['vehicleProfiles']['compact'])
    sources=implementation_files(Path(__file__))+[Path(__file__).with_name('geos_triangulation.py').resolve(),
                    Path(__file__).with_name('transport_network_recipes.json').resolve()]
    checks.update(verify_request_ledger())
    result=dict(checks=checks,places=[],measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},
                reference_sha256=digest(args.reference),frame_sha256=digest(frame_path),
                retained_sha256={str(p):digest(p) for p in args.retain},
                scope='All public-motor routes of every physical module touched by the frozen one-km windows; no other modes or runtime integration.')
    for row in frames['places']:
        if row['place'] not in required:continue
        report=solve_window(args.root,row['place'],row['frames'],required[row['place']],args.output,args.coupled)
        result['places'].append(report)
        result['peak_rss_bytes']=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*(1 if sys.platform=='darwin' else 1024)
        (args.output/'window-roadbeds.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({k:v for k,v in report.items() if k not in ('trials','source_tiles','input_sha256')}),flush=True)


if __name__=='__main__':
    main()
