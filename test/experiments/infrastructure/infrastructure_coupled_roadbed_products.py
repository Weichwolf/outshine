"""Apply cooked coupled roadbeds while preserving the whole source-route cohort."""

import json
from pathlib import Path

import numpy as np
import shapely

from infrastructure_cut_setback_experiment import digest
from infrastructure_cut_setbacks import fork_cut_plan,restore_cut_plan,external_frames_unchanged
from infrastructure_directed_routes import directed_module_routes
from infrastructure_junction_surfaces import motor_surface,same_island_winding
from infrastructure_shared_frames import frame_key
from infrastructure_source_roadbeds import published_roadbed_body_check
from infrastructure_turn_roadbeds import preserves_coverage,published_roadbed_surface


def source_request(poses,pair):
    return tuple(frame_key(poses[i])[:3] for i in pair)


def validate_request_ledger(pairs,proofs,unresolved):
    requests={tuple(map(tuple,p['request'])) for p in proofs}
    omitted={tuple(map(tuple,p['request'])) for p in unresolved}
    assert len(requests)==len(proofs) and len(omitted)==len(unresolved),'duplicate source request'
    assert not requests&omitted and pairs.keys()==requests|omitted,'source requests changed'
    assert all(p.get('reason') for p in unresolved),'missing omission reason'


def prove_product_routes(network,report,floors):
    assert all(p['module'] in floors for p in report['proofs']+report.get('unresolved',[])),'unknown product owner'
    records={}
    for owner in report['affected']:
        poses,_=network.poses(owner,{})
        routes,_=directed_module_routes(network.graph,poses,network.shape(owner),network.tree)
        pairs={source_request(poses,pair):pair for pair in routes}
        proofs=[p for p in report['proofs'] if p['module']==owner]
        unresolved=[p for p in report.get('unresolved',[]) if p['module']==owner]
        validate_request_ledger(pairs,proofs,unresolved)
        turns=[]
        for proof in proofs:
            pair=pairs[tuple(map(tuple,proof['request']))]
            pieces=[dict(p,begin=np.array(p['begin']),end=np.array(p['end']),
                         fit={k:np.asarray(v) for k,v in p['fit'].items()}) for p in proof['pieces']]
            assert np.allclose(pieces[0]['begin'],poses[pair[0]]['position'],atol=1e-7,rtol=0)
            assert np.allclose(pieces[-1]['end'],poses[pair[1]]['position'],atol=1e-7,rtol=0)
            assert all(p['fit']['maximum_curvature_per_m'][0]*network.vehicle['minimumRearAxleRadiusM']<=1+1e-12 for p in pieces)
            body=published_roadbed_body_check(floors[owner],pieces,network.vehicle,network.curbs['arc_error_m'])
            assert body['fits_surface'],(owner,pair,body)
            path=shapely.LineString(proof['path'])
            assert same_island_winding(path,routes[pair],floors[owner]),'source island winding changed'
            turns.append(dict(entry=pair[0],exit=pair[1],accepted=True,reason='coupled_native_mesh_checked',body=body))
        accepted=len(turns)
        for row in unresolved:
            pair=pairs[tuple(map(tuple,row['request']))]
            turns.append(dict(entry=pair[0],exit=pair[1],accepted=False,reason=row['reason']))
        records[owner]=dict(accepted=accepted,connections=len(pairs),turns=sorted(turns,key=lambda t:(t['entry'],t['exit'])))
    return records


def apply_coupled_product(network,records,required,root,path,occupied):
    report=json.loads(path.read_text())
    assert report['place']==network.name and report['vehicle']==network.vehicle
    for name,value in report['input_sha256'].items():assert digest(root/name)==value
    for name,value in report['implementation_sha256'].items():assert digest(Path(name))==value
    assert digest(Path(report['physical_mesh']))==report['physical_mesh_sha256']
    affected=set(report['affected'])
    assert not affected&occupied,'overlapping coupled products require a joint solve'
    sections={row['section'] for row in report['cut_plan']['sections']}
    frame_sections=set(report.get('changed_frame_sections',sections))
    assert sections<=frame_sections,'moved cuts missing from frame scope'
    old_poses={owner:network.poses(owner,{})[0] for owner in affected}
    old_requests={owner:{source_request(poses,pair) for pair in directed_module_routes(
                    network.graph,poses,network.shape(owner),network.tree)[0]} for owner,poses in old_poses.items()}
    needed={key:source_request(old_poses[key[0]],key[1:]) for key in required if key[0] in affected}
    original=shapely.union_all([network.shape(m) for m in affected])
    trial=fork_cut_plan(network,report['module'],sections)
    restore_cut_plan(trial,report['cut_plan'])
    floors={}
    with np.load(report['physical_mesh'],allow_pickle=False) as mesh:
        for row in report['published_surfaces']:
            owner=row['module'];assert owner in affected and owner not in floors
            owned=dict(vertices=mesh['vertices'],indices=mesh['indices'][mesh['face_owners']==owner])
            quantum=row['coordinate_precision_m']
            physical=shapely.union_all(shapely.set_precision(shapely.polygons(owned['vertices'][owned['indices']]),quantum))
            expected=shapely.from_wkb(row['physical_shape_wkb'])
            assert physical.symmetric_difference(expected).area<=1e-8,'native owner mismatch'
            trial.shapes[owner]=physical
            planned=shapely.from_wkb(row['public_surface_wkb'])
            source_floor=motor_surface(planned,trial.graph,trial.tree,trial.mesh,trial.vehicle,
                                       trial.curbs['corner_radius_m'],trial.curbs['arc_error_m'])
            base,core=shapely.set_precision(np.array([source_floor,physical],dtype=object),quantum)
            floors[owner],_=published_roadbed_surface(owned,base,core,planned,quantum)
    assert floors.keys()==affected
    assert preserves_coverage(original,shapely.union_all([trial.shape(m) for m in affected]))
    frames={frame_key(p)[:3]:p for p in report['published_frames']}
    for owner in affected:
        poses,_=trial.poses(owner,{})
        for pose in poses:
            row=frames[frame_key(pose)[:3]]
            assert abs(pose['station']-row['station'])<=1e-7,'source station changed'
            trial.frames[frame_key(pose)]={k:np.array(row[k]) for k in ('position','heading')}
        current,_=trial.poses(owner,{})
        assert external_frames_unchanged(old_poses[owner],current,frame_sections),'external frame changed'
        for pose in current:
            if pose['section'] in frame_sections:
                assert trial.owners[frame_key(pose)]<=affected,'changed frame owner missing'
        requests={source_request(current,pair) for pair in directed_module_routes(
                    trial.graph,current,trial.shape(owner),trial.tree)[0]}
        assert requests==old_requests[owner]
    proved=prove_product_routes(trial,report,floors)
    after=dict(records);remapped=required-needed.keys()
    for owner in affected&records.keys():
        accepted={source_request(trial.poses(owner,{})[0],(t['entry'],t['exit']))
                  for t in proved[owner]['turns'] if t['accepted']}
        previous={source_request(old_poses[owner],(t['entry'],t['exit']))
                  for t in records[owner]['turns'] if t['accepted']}
        assert previous<=accepted,'previous proved routes lost'
        assert {key for old,key in needed.items() if old[0]==owner}<=accepted,'required roadbeds lost'
        after[owner]=dict(records[owner],**proved[owner])
        poses,_=trial.poses(owner,{})
        pairs={source_request(poses,(t['entry'],t['exit'])):(t['entry'],t['exit']) for t in proved[owner]['turns']}
        remapped.update((owner,*pairs[key]) for old,key in needed.items() if old[0]==owner)
    return trial,after,remapped,dict(modules=sorted(affected),proved_routes=len(report['proofs']),
                                  report=str(path),report_sha256=digest(path))
