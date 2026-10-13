import json
from pathlib import Path
import sys
import time
from collections import Counter

import numpy as np
import shapely

sys.path.insert(0,'test/experiments/transport')
from transport_turn_roadbed_experiment import (prepared_network,building_reservations,
    motor_surface,directed_module_routes,construct_turn_roadbed,shared_mesh,
    preserves_voids,preserves_coverage,turn_body_check,digest)
from transport_cut_setbacks import restore_cut_plan
from transport_route_connectivity import network_connectivity
from transport_travel import MODES

root=Path('build/experiments/transport')
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
results=[]
for name in ('ZuerichHauptbahnhof',):
    began=time.perf_counter()
    before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
    cuts=json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text())
    network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
    restore_cut_plan(network,cuts['plan'])
    records={r['module']:r for r in before['modules']}
    records.update((r['module'],r) for r in cuts['changed_modules'])
    current=dict(before,modules=[records[r['module']] for r in before['modules']])
    bounds=shapely.MultiPoint(network.product['vertices']).envelope.buffer(network.vehicle['widthM'])
    receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
    buildings,tiles=building_reservations(receipt,bounds)
    nonmotor=~np.any(network.graph['travel_public']&MODES['motor']!=0,axis=1)
    other=shapely.union_all(shapely.buffer(network.graph['lines'][nonmotor],
                  network.graph['half_widths'][nonmotor],quad_segs=2))
    reserved=buildings.union(other)
    sections=np.array(list(network.sections.values()),dtype=object)
    section_index=shapely.STRtree(sections)
    proposed=[];trials=[]
    changed=set(r['module'] for r in cuts['plan']['parts'])
    for row in current['modules']:
        if row['module']!=1383:
            continue
        if row['accepted']==row['connections']:
            continue
        module=row['module'];shape=network.shape(module);poses,_=network.poses(module,{})
        floor=motor_surface(shape,network.graph,network.tree,network.mesh,network.vehicle,
                            network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
        routes,_=directed_module_routes(network.graph,poses,shape,network.tree)
        for turn in row['turns']:
            if turn['accepted']:
                continue
            pair=turn['entry'],turn['exit']
            start=time.perf_counter()
            candidate,notes=construct_turn_roadbed(poses[pair[0]],poses[pair[1]],floor,reserved,
                         routes[pair],network.vehicle,network.curbs['arc_error_m'])
            note=dict(module=module,entry=pair[0],exit=pair[1],accepted=False,
                      elapsed_ms=(time.perf_counter()-start)*1000,
                      reasons=dict(Counter(r['reason'] for r in notes)))
            trials.append(note)
            if candidate is None:
                note['reason']='no_constructive_candidate'
                continue
            addition=candidate['addition']
            ids=network.mesh[1].query(addition,predicate='intersects')
            owners=set(map(int,network.product['face_owners'][ids]))|changed
            if any(addition.intersection(network.shape(owner)).area>1e-8
                   for owner in owners if owner!=module):
                note['reason']='another_physical_owner'
                continue
            if len(section_index.query(addition,predicate='dwithin',distance=1e-7)):
                note['reason']='touches_shared_section'
                continue
            note['reason']='pending_combined_native_mesh'
            proposed.append(dict(module=module,pair=pair,candidate=candidate,note=note,
                                 poses=poses,shape=shape,floor=floor))
    additions=np.array([p['candidate']['addition'] for p in proposed],dtype=object)
    index=shapely.STRtree(additions)
    selected=set(); rejected=set()
    for i in sorted(range(len(proposed)),key=lambda k:(additions[k].area,proposed[k]['module'],proposed[k]['pair'])):
        p=proposed[i]
        conflicts=[int(j) for j in index.query(additions[i],predicate='intersects')
                   if int(j) in selected and proposed[int(j)]['module']!=p['module']
                   and additions[i].intersection(additions[int(j)]).area>1e-8]
        if conflicts:
            p['note']['reason']='conflicts_with_selected_addition';rejected.add(i)
        else:
            selected.add(i)
    precision=json.loads((root/'directed-surfaces'/f'{name}-flat-surfaces.json').read_text())['precision_m']
    for module in sorted({proposed[i]['module'] for i in selected}):
        choices=[proposed[i] for i in sorted(selected) if proposed[i]['module']==module]
        first=choices[0]
        floor,shape=first['floor'],first['shape']
        added=shapely.union_all([p['candidate']['addition'] for p in choices])
        planned,physical=floor.union(added),shape.union(added)
        try:
            assert preserves_voids(floor,planned) and preserves_voids(shape,physical),'changed_void_topology'
            parts=shapely.get_parts(physical)
            mesh,audit=shared_mesh(physical,parts,np.ones(len(parts),dtype=int),precision)
            actual=shapely.union_all(shapely.polygons(mesh['vertices'][mesh['indices']]))
            complete=actual.intersection(planned).union(floor.difference(shape))
            assert preserves_coverage(floor,complete),'lost_original_public_floor'
            for p in choices:
                pair=p['pair'];poses=p['poses']
                body,_=turn_body_check(complete,p['candidate']['fit'],poses[pair[0]]['position'],
                          poses[pair[1]]['position'],'G2',network.vehicle,network.curbs['arc_error_m'])
                if not body['fits_surface']:
                    target=root/'roadbeds/body-rejection'/f'{name}-{module}-{pair[0]}-{pair[1]}.json'
                    target.parent.mkdir(parents=True,exist_ok=True)
                    sample=dict(shape_wkb=shape.wkb.hex(),floor_wkb=floor.wkb.hex(),planned_wkb=planned.wkb.hex(),
                        complete_wkb=complete.wkb.hex(),physical_wkb=physical.wkb.hex(),actual_wkb=actual.wkb.hex(),
                        expected_wkb=p['candidate']['surface'].wkb.hex(),fit=p['candidate']['fit'],
                        begin=poses[pair[0]]['position'],end=poses[pair[1]]['position'],vehicle=network.vehicle,
                        error=network.curbs['arc_error_m'],body=body)
                    target.write_text(json.dumps(sample,default=lambda obj:obj.tolist())+'\n')
                    delta=p['candidate']['surface'].difference(complete)
                    p['note'].update(failed_body=body,missing_floor_m2=delta.area,
                                    missing_floor_bounds=list(delta.bounds))
                assert body['fits_surface'],'published_mesh_does_not_support_body'
            path=root/'roadbeds/window-native'/f'{name}-{module}.npz'
            path.parent.mkdir(parents=True,exist_ok=True)
            np.savez_compressed(path,vertices=mesh['vertices'],indices=mesh['indices'])
            for p in choices:
                p['note'].update(accepted=True,reason='combined_native_mesh_checked',
                    added_area_m2=p['candidate']['addition'].area,mesh=str(path),mesh_sha256=digest(path))
            proved={p['pair'] for p in choices}
            original=records[module]
            records[module]=dict(original,accepted=original['accepted']+len(proved),
                      turns=[dict(t,accepted=True) if (t['entry'],t['exit']) in proved else t for t in original['turns']])
        except (AssertionError,ValueError) as error:
            for p in choices:p['note'].update(reason='combined_native_rejection',detail=str(error))
    after=dict(current,modules=[records[r['module']] for r in current['modules']])
    before_graph=network_connectivity(network,current);after_graph=network_connectivity(network,after)
    result=dict(place=name,trials=trials,proposed=len(proposed),native_accepted=sum(t['accepted'] for t in trials),
         before=sum(r['accepted'] for r in current['modules']),after=sum(r['accepted'] for r in after['modules']),
         connections=sum(r['connections'] for r in after['modules']),
         weak_components=[before_graph['proved_weak_components'],after_graph['proved_weak_components']],
         strong_components=[before_graph['proved_strong_components'],after_graph['proved_strong_components']],
         reasons=dict(Counter(t['reason'] for t in trials if not t['accepted'])),
         elapsed_ms=(time.perf_counter()-began)*1000,source_tiles=tiles,
         input_sha256=before['input_sha256'])
    results.append(result)
    print(json.dumps({k:v for k,v in result.items() if k not in ('trials','source_tiles','input_sha256')}),flush=True)
    (root/'roadbeds/body-rejection-probe-results.json').write_text(json.dumps(results,indent=2)+'\n')
