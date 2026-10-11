"""Measure shared-cut corrections against frozen complete motor-route windows."""

import argparse
import ast
from collections import Counter
import hashlib
import json
from pathlib import Path
import resource
import subprocess
import sys
import time
from types import SimpleNamespace

import numpy as np
import shapely

from infrastructure_cut_setbacks import (CutSetbackRejected,setback_shared_section,setback_network,
    route_signatures,external_frames_unchanged,serialize_cut_plan,restore_cut_plan,check_contact_progress)
from infrastructure_driving_network import DrivingNetwork
from infrastructure_hermite_experiment import fit_module
from infrastructure_route_connectivity import network_connectivity
from infrastructure_shared_frames import frame_key


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):
            result.update(block)
    return result.hexdigest()


def implementation_files(entry):
    pending,visited = [entry.resolve()],set()
    while pending:
        path = pending.pop()
        if path in visited:
            continue
        visited.add(path)
        statements = list(ast.walk(ast.parse(path.read_text())))
        names = {n.module.split('.')[0] for n in statements if isinstance(n,ast.ImportFrom) and n.module}
        names.update(a.name.split('.')[0] for n in statements if isinstance(n,ast.Import) for a in n.names)
        pending.extend(path.with_name(name+'.py') for name in names
                       if name.startswith('infrastructure_') and path.with_name(name+'.py').is_file())
    return sorted(visited)


def verify_contact_progress():
    network=SimpleNamespace(graph=dict(lines=[shapely.LineString([[0,0],[10,0]]),
                                             shapely.LineString([[10,2],[0,2]])]))
    a=dict(section=1,edge=0,direction=1,station=2.,incoming=False)
    b=dict(section=1,edge=1,direction=1,station=8.,incoming=True)
    new=[dict(a,station=3.),dict(b,station=7.)]
    check_contact_progress(network,[a,b],new,{1})
    for bad in (new[:1],new+[new[0]],[new[0],dict(new[1],station=9.)],
                [new[0],dict(new[1],station=0.)],[new[0],dict(new[1],incoming=False)]):
        try:
            check_contact_progress(network,[a,b],bad,{1})
        except CutSetbackRejected:
            continue
        raise AssertionError('invalid source contact update accepted')
    return dict(multi_axis_progress_checked=True,contact_loss_rejected=True,
                reversed_contact_progress_rejected=True)


def verify_setbacks():
    core,road = shapely.box(-5,-3,0,3),shapely.box(0,-3,20,3)
    section = shapely.LineString([[0.,-3.],[0.,3.]])
    tangent = np.array([2.,0.])
    original = tangent.copy()
    for angle,offset in ((0.,[0.,0.]),(.7,[10000.,-7000.]),(2.1,[-300.,50.])):
        matrix = np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
        transform = lambda xy:np.einsum('ij,kj->ki',matrix,xy)+offset
        a,b = shapely.transform(core,transform),shapely.transform(road,transform)
        point = transform(np.array([[4.,0.]]))[0]
        new_a,new_b,moved_section = setback_shared_section(a,b,shapely.transform(section,transform),point,matrix@tangent)
        assert np.allclose([new_a.area,new_b.area],[54.,96.],atol=1e-8,rtol=0)
        assert moved_section.distance(shapely.Point(point))<1e-7 and abs(moved_section.length-6)<1e-7
        assert shapely.hausdorff_distance(new_a,shapely.transform(shapely.box(-5,-3,4,3),transform))<1e-7
        assert shapely.intersection(new_a,new_b).area<1e-8
    setback_shared_section(core,road,section,np.array([4.,0.]),tangent)
    assert np.array_equal(tangent,original)
    for point in ([-2.,0.],[24.,0.],[20.,0.]):
        try:
            setback_shared_section(core,road,section,np.array(point),tangent)
        except CutSetbackRejected:
            continue
        raise AssertionError('an invalid cut was accepted')
    holes = shapely.Polygon([[-5,-3],[20,-3],[20,3],[-5,3]],holes=[[[10,-1],[12,-1],[12,1],[10,1]]])
    a,b,section = setback_shared_section(core,shapely.difference(holes,core),section,np.array([4.,0.]),tangent)
    assert len(b.interiors)==1 and not shapely.covers(shapely.union_all([a,b]),shapely.Point(11,0))
    pose = dict(section=1,edge=7,direction=1,station=5.,position=np.array([0.,0.]),heading=np.array([1.,0.]))
    shifted = dict(pose,station=9.,position=np.array([4.,0.]))
    report = dict(turns=[dict(entry=0,exit=1,accepted=True)])
    other = dict(pose,section=2,edge=8)
    assert route_signatures(report,[pose,other])==route_signatures(report,[shifted,other])
    assert external_frames_unchanged([pose,other],[shifted,other],{1})
    assert not external_frames_unchanged([pose,other],[pose,dict(other,heading=np.array([0.,1.]))],{1})
    return dict(surface_partition_preserved=True,holes_preserved=True,straight_shared_boundary=True,
                rigid_transform_checked=True,input_tangent_unchanged=True,source_contact_identity_preserved=True,
                changed_external_frame_rejected=True,invalid_setbacks_rejected=True,**verify_contact_progress())


def verify_loop_setbacks():
    area = shapely.box(-10,-10,10,10).difference(shapely.box(-6,-6,6,6))
    core = area.intersection(shapely.box(-20,-20,0,20))
    road = area.difference(core)
    selected = shapely.LineString([[0.,-10.],[0.,-6.]])
    retained = shapely.LineString([[0.,6.],[0.,10.]])
    for angle,offset in ((0.,[0.,0.]),(.7,[10000.,-7000.]),(2.1,[-300.,50.])):
        matrix = np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
        transform = lambda xy:xy@matrix.T+offset
        a,b,line = (shapely.transform(s,transform) for s in (core,road,selected))
        point = transform(np.array([[4.,-8.]]))[0]
        moved_a,moved_b,cut = setback_shared_section(a,b,line,point,matrix@np.array([1.,0.]))
        assert np.allclose([moved_a.area,moved_b.area],[144.,112.],atol=1e-7,rtol=0)
        assert moved_a.union(moved_b).symmetric_difference(shapely.transform(area,transform)).area<1e-7
        assert moved_a.intersection(moved_b).area==0
        assert len(moved_a.union(moved_b).interiors)==1
        other = shapely.transform(retained,transform)
        shared = moved_a.boundary.intersection(moved_b.boundary)
        assert shapely.covered_by(other,shapely.buffer(shared,1e-7))
        assert shapely.hausdorff_distance(shared,shapely.union_all([other,cut]))<1e-7
    try:
        setback_shared_section(core,road,selected,np.array([8.,-8.]),np.array([1.,0.]))
    except CutSetbackRejected:
        pass
    else:
        raise AssertionError('a cut across an unrelated loop arm was accepted')
    return dict(loop_other_contact_preserved=True,loop_hole_preserved=True,local_ownership_transfer_checked=True)


def prepared_network(root,report,frames):
    for name,value in report['input_sha256'].items():
        assert digest(root/name)==value,'source inputs changed since the frozen window'
    network = DrivingNetwork(root,report['place'],json.loads(Path(__file__).with_name(
                             'infrastructure_network_recipes.json').read_text())['vehicleProfiles']['compact'])
    network.frames = {frame_key(f):{k:np.array(f[k]) for k in ('position','heading')} for f in frames}
    return network


def try_setback(network,module,pair,records,maximum_calls):
    poses,_ = network.poses(module,{})
    sections = {poses[i]['section'] for i in pair}
    required = next(iter(route_signatures(dict(turns=[dict(entry=pair[0],exit=pair[1])]),poses)))
    trials = []
    for factor in (1.,2.,3.):
        started = time.perf_counter()
        distance = factor*network.vehicle['minimumRearAxleRadiusM']
        note = dict(module=module,entry=pair[0],exit=pair[1],setback_m=distance,accepted=False)
        try:
            trial,changes,affected = setback_network(network,module,sections,distance)
            before = {m:records[m] if m in records else fit_module(network,m,maximum_calls)[0][0] for m in affected}
            original = {m:network.poses(m,{})[0] for m in affected}
            solved = {m:fit_module(trial,m,maximum_calls)[0] for m in sorted(affected)}
            requests = all(route_signatures(before[m],original[m])==route_signatures(solved[m][0],solved[m][1][2]) for m in affected)
            retained = all(route_signatures(before[m],original[m],True)<=route_signatures(solved[m][0],solved[m][1][2],True) for m in affected)
            external = all(external_frames_unchanged(original[m],solved[m][1][2],sections) for m in affected)
            gained = required in route_signatures(solved[module][0],solved[module][1][2],True)
            note.update(same_requests=requests,previous_routes_preserved=retained,external_frames_unchanged=external,
                        target_solved=gained,affected_modules=sorted(affected),
                        before_accepted=sum(before[m]['accepted'] for m in affected),
                        after_accepted=sum(solved[m][0]['accepted'] for m in affected),
                        connections=sum(before[m]['connections'] for m in affected),
                        accepted=requests and retained and external and gained)
            if not note['accepted']:
                note['reason'] = ('source_requests_changed' if not requests else 'neighbor_route_lost' if not retained
                                  else 'external_frame_changed' if not external else 'target_still_unsolved')
            note['elapsed_ms'] = (time.perf_counter()-started)*1000
            trials.append(note)
            print(json.dumps(dict(place=network.name,**note)),flush=True)
            if note['accepted']:
                trial.records.update(solved)
                return trial,changes,solved,trials
        except CutSetbackRejected as error:
            note.update(reason=str(error),elapsed_ms=(time.perf_counter()-started)*1000)
            trials.append(note)
            print(json.dumps(dict(place=network.name,**note)),flush=True)
    return network,[],{},trials


def correction_targets(network,connectivity,selected):
    critical = {}
    for edge in connectivity['missing_edges']:
        if edge['reason']=='disconnected':
            for route in edge['routes']:
                if route['module'] in network.junctions:
                    critical.setdefault(route['module'],(route['entry'],route['exit']))
    if selected:
        return {m:critical[m] for m in sorted(selected) if m in critical}
    return dict(sorted(critical.items()))


def updated_rows(report,changed):
    rows = {r['module']:r for r in report['modules']}
    rows.update((m,r[0]) for m,r in changed.items() if m in rows)
    return [rows[r['module']] for r in report['modules']]


def survey(root,report,frames,selected,maximum_calls):
    started = time.perf_counter()
    usage = resource.getrusage(resource.RUSAGE_SELF)
    network = prepared_network(root,report,frames)
    before_graph = network_connectivity(network,report)
    targets = correction_targets(network,before_graph,selected)
    records = {r['module']:r for r in report['modules']}
    changed,changes,trials = {},[],[]
    for module,pair in targets.items():
        network,local,solved,notes = try_setback(network,module,pair,records,maximum_calls)
        changes.extend(local)
        changed.update(solved)
        records.update((m,r[0]) for m,r in solved.items())
        trials.extend(notes)
    current = dict(report,modules=updated_rows(report,changed))
    after_graph = network_connectivity(network,current)
    assert before_graph['nodes']==after_graph['nodes']
    assert before_graph['requested_edges']==after_graph['requested_edges']
    assert after_graph['proved_edges']>=before_graph['proved_edges']
    plan = serialize_cut_plan(network,changes)
    restored = prepared_network(root,report,frames)
    restore_cut_plan(restored,plan)
    for module,(row,view) in changed.items():
        poses,_ = restored.poses(module,{})
        assert len(poses)==len(view[2])
        assert all(frame_key(a)==frame_key(b) and np.array_equal(a['position'],b['position']) and
                   np.array_equal(a['heading'],b['heading']) for a,b in zip(poses,view[2]))
        assert shapely.equals_exact(restored.shape(module),view[0],0)
    ended = resource.getrusage(resource.RUSAGE_SELF)
    return dict(place=report['place'],radius_m=report['radius_m'],input_sha256=report['input_sha256'],
                before=report['after'],after=dict(modules=len(current['modules']),
                  connections=sum(r['connections'] for r in current['modules']),
                  accepted=sum(r['accepted'] for r in current['modules'])),
                before_graph=before_graph,after_graph=after_graph,plan=plan,trials=trials,
                changed_modules=[row for row,_ in changed.values()],
                changed_paths=[dict(module=m,paths=[None if p is None else shapely.get_coordinates(p).tolist()
                                                   for p in view[3]]) for m,(_,view) in changed.items()],
                exact_plan_roundtrip=True,elapsed_ms=(time.perf_counter()-started)*1000,
                cpu_user_ms=(ended.ru_utime-usage.ru_utime)*1000,cpu_system_ms=(ended.ru_stime-usage.ru_stime)*1000,
                process_peak_rss_bytes=ended.ru_maxrss*(1 if sys.platform=='darwin' else 1024),targets=len(targets),
                rejected_reasons=dict(Counter(t['reason'] for t in trials if not t['accepted'])))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('places',nargs='*',default=['Wien','ZuerichHauptbahnhof','BaselBadischerBahnhof'])
    parser.add_argument('--input',type=Path,default=Path('build/experiments/infrastructure'))
    parser.add_argument('--reference',type=Path,default=Path('build/experiments/infrastructure/window-1000-body'))
    parser.add_argument('--frames',type=Path,default=Path('build/experiments/infrastructure/boundary-optimized/hermite-routes.json'))
    parser.add_argument('--output',type=Path,default=Path('build/experiments/infrastructure/cut-setbacks'))
    parser.add_argument('--modules',nargs='*',default=[])
    parser.add_argument('--maximum-calls',type=int,default=1024)
    parser.add_argument('--check-only',action='store_true')
    args = parser.parse_args()
    checks = verify_setbacks()|verify_loop_setbacks()
    if args.check_only:
        print(json.dumps(checks))
        return
    args.output.mkdir(parents=True,exist_ok=True)
    frames = json.loads(args.frames.read_text())
    code = Path(__file__).parent
    configuration = code/'infrastructure_network_recipes.json'
    assert digest(configuration)==frames['implementation_sha256'][configuration.name]
    math_files = implementation_files(Path(__file__))+[configuration]
    assert Path(__file__).with_name('infrastructure_cut_setback_render.py').resolve() not in math_files
    result = dict(checks=checks,places=[],implementation_sha256={f.name:digest(f) for f in math_files},
                  frame_reference=str(args.frames.relative_to(args.input)),frame_reference_sha256=digest(args.frames),
                  measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                  scope='Original complete motor windows; shared source cuts move within existing surfaces. Other modes and 2.5D remain open.')
    for name in args.places:
        reference = args.reference/f'{name}-routes.json'
        selected = {int(p.split(':')[1]) for p in args.modules if p.split(':')[0]==name}
        if args.modules and not selected:
            continue
        baseline = json.loads(reference.read_text())
        row = survey(args.input,baseline,next(p['frames'] for p in frames['places'] if p['place']==name),selected,args.maximum_calls)
        row.update(reference=str(reference.relative_to(args.input)),reference_sha256=digest(reference))
        result['places'].append(row)
        (args.output/f'{name}-cut-setbacks.json').write_text(json.dumps(row,indent=2)+'\n')
        print(json.dumps(dict(place=name,before=row['before'],after=row['after'],targets=row['targets'],
            weak_components=[row[k]['proved_weak_components'] for k in ('before_graph','after_graph')],
            elapsed_ms=row['elapsed_ms'])),flush=True)
    (args.output/'cut-setbacks.json').write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':
    main()
