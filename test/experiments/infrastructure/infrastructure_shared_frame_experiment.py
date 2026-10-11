"""Negotiate real shared lane frames and verify every affected neighboring module."""

import argparse
from collections import Counter
import hashlib
from itertools import product,islice
import json
from pathlib import Path
import time

import numpy as np
import shapely

from infrastructure_driving_network import DrivingNetwork
from infrastructure_junction_turn_experiment import selected_junctions,render
from infrastructure_shared_frames import (frame_key,heading_choices,choose_shared_frames,accepted_routes,preserves_routes,
                                          section_pose_domain,ranked_pairs,pose_cost)
from infrastructure_vehicle_sweep import body_outlines


def verify_selection(vehicle):
    entry = dict(section=1,edge=4,direction=1,station=3.,position=np.array([-5.,0.]),heading=np.array([1.,0.]))
    exit = dict(section=2,edge=7,direction=-1,station=2.,position=np.array([5.,0.]),heading=np.array([1.,0.]))
    report = lambda accepted:dict(turns=[dict(entry=0,exit=1,accepted=accepted)])
    before = {10:report(False),20:report(True)}
    angle = np.deg2rad(5.)
    frame = lambda p,h:dict(position=p['position'],heading=h)
    options = [[(frame(entry,entry['heading']),0.)],[(frame(exit,np.array([np.cos(angle),-np.sin(angle)])),angle),
                                     (frame(exit,np.array([np.cos(angle),np.sin(angle)])),angle)]]
    evaluate = lambda overrides:{10:report(True),20:report(overrides[frame_key(exit)]['heading'][1]>0)}
    overrides,after,stats = choose_shared_frames(entry,exit,options,before,lambda a,b:True,evaluate)
    assert stats['accepted'] and stats['rejected_loss']==1 and overrides[frame_key(exit)]['heading'][1]>0
    assert preserves_routes(before,after) and np.array_equal(exit['heading'],[1.,0.])
    rejected,unchanged,note = choose_shared_frames(entry,exit,options,before,lambda a,b:True,evaluate,1)
    assert not rejected and unchanged is before and note['reason']=='frame_candidate_budget_exhausted'
    assert not preserves_routes(before,{10:report(True),20:dict(turns=[])})
    unrelated = {**before,30:report(False)}
    result,unchanged,note = choose_shared_frames(entry,exit,options,unrelated,lambda a,b:True,
                            lambda overrides:{10:report(False),20:report(True),30:report(True)},
                            required_gain=(10,0,1))
    assert not result and unchanged is unrelated and note['rejected_target']==2
    assert frame_key(entry)!=frame_key(dict(entry,station=4.))
    assert frame_key(entry)!=frame_key(dict(entry,direction=-1))
    floor = shapely.box(-20,-1.2,20,1.2)
    choices = heading_choices(entry,entry['heading'],[floor],vehicle,.025)
    assert choices
    for heading,_ in choices:
        rectangle = shapely.Polygon(body_outlines(entry['position'][None,:],
                                  [np.arctan2(heading[1],heading[0])],vehicle['widthM']*.5,
                                  vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])[0])
        assert shapely.covers(floor,rectangle) and floor.boundary.distance(rectangle)>=.025
    rotation = np.array([[.6,-.8],[.8,.6]])
    transform = lambda xy:np.einsum('ij,kj->ki',rotation,xy)+[10000.,-7000.]
    moved = dict(entry,position=transform(entry['position'][None,:])[0],heading=rotation@entry['heading'])
    rotated = heading_choices(moved,moved['heading'],[shapely.transform(floor,transform)],vehicle,.025)
    assert len(choices)==len(rotated)
    assert all(np.allclose(rotation@a,b,atol=1e-10) for (a,_),(b,_) in zip(choices,rotated))
    return dict(neighbor_loss_rejected=True,rollback_on_budget=True,requests_preserved=True,
                requested_gain_required=True,multiple_contacts_distinct=True,
                static_body_checked=True,rigid_transform_invariant=True)


def verify_domains(vehicle):
    half,error = 2.75,.025
    floor = shapely.box(-25,-half,25,half)
    pose = dict(position=np.array([0.,-half*.5]),heading=np.array([1.,0.]))
    lane = dict(section=shapely.LineString([[0,-half],[0,half]]),heading=pose['heading'],
                origin=np.zeros(2),half_width=half,separated=True)
    domain = section_pose_domain(pose,lane,[floor],vehicle,error)
    assert domain and any(np.linalg.norm(p['position']-pose['position'])>.1 for p in domain)
    for frame in domain:
        body = shapely.Polygon(body_outlines(frame['position'][None,:],
                              [np.arctan2(frame['heading'][1],frame['heading'][0])],vehicle['widthM']*.5,
                              vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])[0])
        crossing = shapely.get_coordinates(shapely.intersection(body,lane['section']))
        assert lane['section'].distance(shapely.Point(frame['position']))<1e-10
        assert shapely.covers(floor,body) and floor.boundary.distance(body)>=error
        assert np.all((-crossing[:,1]>=error)&(-crossing[:,1]<=half-error))
        assert -frame['position'][1]>=vehicle['widthM']*.5+error
        assert pose_cost(frame,pose,vehicle)>=0
    rotation = np.array([[.6,-.8],[.8,.6]])
    transform = lambda xy:np.einsum('ij,kj->ki',rotation,xy)+[10000.,-7000.]
    moved = dict(position=transform(pose['position'][None,:])[0],heading=rotation@pose['heading'])
    shifted = dict(lane,section=shapely.transform(lane['section'],transform),heading=moved['heading'],
                   origin=transform(lane['origin'][None,:])[0])
    rotated = section_pose_domain(moved,shifted,[shapely.transform(floor,transform)],vehicle,error)
    assert len(domain)==len(rotated)
    for a,b in zip(domain,rotated):
        assert np.allclose(transform(a['position'][None,:])[0],b['position'],atol=1e-9)
        assert np.allclose(rotation@a['heading'],b['heading'],atol=1e-10)
        assert abs(pose_cost(a,pose,vehicle)-pose_cost(b,moved,vehicle))<1e-9
    mirror = np.array([1.,-1.])
    left = section_pose_domain(dict(position=pose['position']*mirror,heading=pose['heading']),lane,[floor],vehicle,error)
    assert len(left)==len(domain)
    for a in domain:
        assert any(np.linalg.norm(a['position']*mirror-b['position'])<1e-9 and
                   np.linalg.norm(a['heading']*mirror-b['heading'])<1e-9 for b in left)
    return dict(domain_candidates=len(domain),cut_and_lane_preserved=True,
                domain_rigid_transform_invariant=True,body_crossing_checked=True,left_and_right_traffic=True)


def verify_pair_ranking():
    rng = np.random.default_rng(2343)
    cases = 0
    for count in range(128):
        choices = [[(i,float(c)) for i,c in enumerate(rng.integers(0,8,size=int(rng.integers(1,15))))] for _ in range(2)]
        expected = sorted(product(*choices),key=lambda pair:sum(c[1] for c in pair))
        assert list(ranked_pairs(choices))==expected
        cases += 1
    wide = [[(i,float(i)) for i in range(10000)] for _ in range(2)]
    prefix = list(islice(ranked_pairs(wide),64))
    assert len(prefix)==64 and sum(c[1] for c in prefix[-1])<=11
    assert list(ranked_pairs([[],[(0,0.)]]))==[]
    return dict(pair_ranking_cases=cases,cartesian_order_preserved=True,lazy_pair_budget=True)


def near_corridors(network,limit):
    candidates = np.array([k for k,v in network.corridors.items() if len(v)>1],dtype=int)
    first = network.faces[0][network.faces[1][candidates]]
    centres = network.product['vertices'][network.product['indices'][first]].mean(axis=1)
    selected,excluded = [],[]
    for module in candidates[np.argsort(np.linalg.norm(centres,axis=1))]:
        poses,_ = network.poses(int(module),{})
        if (len({p['section'] for p in poses})<2 or not any(p['incoming'] for p in poses) or
                not any(not p['incoming'] for p in poses)):
            excluded.append(int(module))
            continue
        selected.append(int(module))
        if len(selected)>=limit:
            break
    return selected,excluded


def summarize(reports):
    counts = Counter(modules=0,connections=0,accepted=0)
    for report in reports:
        counts.update(modules=1,connections=report['connections'],accepted=report['accepted'])
    return dict(counts)


def verify_actual_frames(network):
    for key,frame in network.frames.items():
        sides = [p for module in network.owners[key] for p in network.record(module)[1][2] if frame_key(p)==key]
        assert len(sides)==2 and sides[0]['incoming']!=sides[1]['incoming']
        assert np.array_equal(sides[0]['position'],sides[1]['position'])
        assert all(np.array_equal(p['heading'],frame['heading']) and
                   np.array_equal(p['position'],frame['position']) for p in sides)
    return dict(shared_frames_checked=len(network.frames),opposed_entry_roles=True,identical_shared_poses=True)


def survey(root,name,vehicle,corridor_limit,junction_limit,candidate_limit):
    began = time.perf_counter()
    network = DrivingNetwork(root,name,vehicle)
    corridors,excluded = near_corridors(network,corridor_limit)
    junctions = [module for module,_,_,_ in selected_junctions(network.product,network.graph,network.tree,
                   network.junctions,network.faces,vehicle,junction_limit)]
    selected = corridors+junctions
    before = {module:network.record(module) for module in selected}
    baseline_ms = (time.perf_counter()-began)*1000
    changes = []
    for index,module in enumerate(selected):
        changes.extend(network.improve(module,candidate_limit))
        if (index+1)%32==0:
            print(json.dumps(dict(place=name,modules_checked=index+1,candidate_trials=sum(c['tried'] for c in changes))),flush=True)
    after_heading = {module:network.record(module) for module in selected}
    for index,module in enumerate(selected):
        changes.extend(network.improve(module,candidate_limit,'position'))
        if (index+1)%32==0:
            print(json.dumps(dict(place=name,pose_modules_checked=index+1,
                                 candidate_trials=sum(c['tried'] for c in changes))),flush=True)
    after = {module:network.record(module) for module in selected}
    assert preserves_routes({k:r[0] for k,r in before.items()},{k:r[0] for k,r in after.items()})
    gains = [(k,*pair) for k in selected for pair in sorted(accepted_routes(after[k][0])-accepted_routes(before[k][0]))]
    comparisons = {kind:dict(before=summarize(before[k][0] for k in ids),
                   after_heading=summarize(after_heading[k][0] for k in ids),after=summarize(after[k][0] for k in ids))
                   for kind,ids in [('corridors',corridors),('junctions',junctions)]}
    gained_modules = list(dict.fromkeys(g[0] for g in gains))
    display = gained_modules[:2]
    views = [((name+' before',k,*before[k]),(name+' after',k,*after[k])) for k in display]
    report = dict(place=name,comparisons=comparisons,gains=gains,losses=[],changes=changes,
                  shared_frame_checks=verify_actual_frames(network),
                  modules=[network.record(k)[0] for k in selected],neighbors_checked=len(network.records),
                  excluded_corridors=excluded,baseline_ms=baseline_ms,total_ms=(time.perf_counter()-began)*1000,
                  frames=[dict(section=k[0],edge=k[1],direction=k[2],station=k[3],
                               position=v['position'].tolist(),heading=v['heading'].tolist()) for k,v in network.frames.items()])
    return report,views


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,default=Path('build/experiments/infrastructure'))
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--shots',type=Path)
    parser.add_argument('--corridors',type=int,default=96)
    parser.add_argument('--junctions',type=int,default=48)
    parser.add_argument('--candidates',type=int,default=64)
    parser.add_argument('places',nargs='*')
    args = parser.parse_args()
    if min(args.corridors,args.junctions,args.candidates)<1:
        parser.error('module and candidate limits must be positive')
    for name in args.places:
        for folder,suffix in [('directed-graphs','flat-network'),('directed-surfaces','flat-surfaces')]:
            for extension in ('npz','json'):
                path = args.input/folder/f'{name}-{suffix}.{extension}'
                if not path.is_file():
                    parser.error(f'missing prepared input: {path}')
    vehicle = json.loads(Path(__file__).with_name('infrastructure_network_recipes.json').read_text())['vehicleProfiles']['compact']
    checks,reports,views = {**verify_selection(vehicle),**verify_domains(vehicle),**verify_pair_ranking()},[],[]
    for name in args.places:
        report,local = survey(args.input,name,vehicle,args.corridors,args.junctions,args.candidates)
        reports.append(report)
        if len(views)<3:
            views.extend(local[:3-len(views)])
        print(json.dumps(dict(place=name,comparisons=report['comparisons'],gains=len(report['gains']),
                             neighbors_checked=report['neighbors_checked'],total_ms=report['total_ms'])),flush=True)
    names = ('infrastructure_shared_frames.py','infrastructure_driving_network.py','infrastructure_corridor_experiment.py',
             'infrastructure_directed_routes.py','infrastructure_port_lanes.py','infrastructure_junction_turn_experiment.py',
             'infrastructure_route_turns.py','infrastructure_route_fairing.py','infrastructure_spline_routes.py',
             'infrastructure_vehicle_sweep.py','infrastructure_clothoids.py','infrastructure_clothoid_turns.py',
             'infrastructure_curvature_paths.py','infrastructure_junction_surfaces.py','infrastructure_junction_turns.py',
             'infrastructure_turn_paths.py','infrastructure_network_recipes.json')
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'shared-frames.json').write_text(json.dumps(dict(checks=checks,places=reports,
        implementation_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
                              (Path(__file__),*(Path(__file__).with_name(n) for n in names))},
        scope='Selected ground motor routes and every changed neighboring module; traffic conflicts and full networks remain open.'),indent=2)+'\n')
    if args.shots and views:
        args.shots.parent.mkdir(parents=True,exist_ok=True)
        render(args.shots,[v[0] for v in views]+[v[1] for v in views],
               'Shared lane frames: every neighboring route retained; additional clear drives in blue')
    print(json.dumps(checks),flush=True)


if __name__=='__main__':
    main()
