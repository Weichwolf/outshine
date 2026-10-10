"""Verify local Hermite fitting on frozen shared frames and unchanged road surfaces."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import resource
import sys
import time

import numpy as np
import shapely
from scipy.interpolate import BPoly

from infrastructure_driving_network import DrivingNetwork
from infrastructure_directed_routes import directed_module_routes
from infrastructure_shared_frames import accepted_routes,frame_key,preserves_routes
from infrastructure_hermite_route_fitting import hermite_controls,residual_dependencies,fit_hermite_route,fitted_route_candidates
from infrastructure_junction_turn_experiment import render
from infrastructure_surface_clearance import SurfaceClearance


def verify_clearance():
    polygon = shapely.Polygon([[-5,-5],[5,-5],[5,5],[-5,5]],holes=[[[-2,-2],[2,-2],[2,2],[-2,2]]])
    surface = shapely.union_all([polygon,shapely.box(8,-3,12,3),shapely.box(-8,4,-3,8)])
    rng = np.random.default_rng(2343)
    cases,samples = 0,0
    for angle,translation in ((0.,[0.,0.]),(.7,[100.,-300.]),(2.3,[10000.,-20000.])):
        matrix = np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
        transform = lambda xy:np.einsum('ij,kj->ki',matrix,xy)+translation
        moved = shapely.transform(surface,transform)
        clearance = SurfaceClearance(moved)
        for margin in (0.,.025,.075,.7):
            xy = rng.uniform([-10,-8],[14,10],(2048,2))
            offsets = margin+np.array([-1e-12,0.,1e-12])
            probes = np.array([[5-offset,y] for offset in offsets for y in (-4.,0.,4.)]+
                              [[2+offset,y] for offset in offsets for y in (-1.,0.,1.)])
            points = shapely.points(transform(np.vstack((xy,probes))))
            distance = shapely.distance(points,shapely.boundary(moved))
            expected = np.maximum(margin-np.where(shapely.covers(moved,points),distance,-distance),0)
            assert np.array_equal(clearance.shortfall(points,margin),expected)
            samples += len(points)
            cases += 1
    clearance = SurfaceClearance(shapely.box(-100,-100,100,100))
    points = shapely.points(rng.uniform(-10,10,(1024,2)))
    assert np.all(clearance.shortfall(points,.075)==0)
    assert clearance.samples==1024 and clearance.measured_samples==0
    assert len(clearance.shortfall(shapely.points(np.empty((0,2))),.075))==0
    return dict(clearance_cases=cases,clearance_samples=samples,whole_boundary_bitwise_equal=True,
                holes_and_components_preserved=True,threshold_and_transform_checked=True,interior_distance_work_skipped=True)


def verify_hermite():
    rng = np.random.default_rng(2343)
    state = rng.normal(size=(18,6))
    controls = hermite_controls(state)
    polynomial = BPoly(controls.transpose(1,0,2)[:,None,:,:],[0,1])
    for order in (0,1,2):
        values = polynomial.derivative(order)
        assert np.allclose(values(0),state[:-1,order*2:order*2+2],atol=1e-13,rtol=0)
        assert np.allclose(values(1),state[1:,order*2:order*2+2],atol=1e-13,rtol=0)
        assert np.allclose(values(1)[:-1],values(0)[1:],atol=1e-13,rtol=0)
    rotation = np.array([[.6,-.8],[.8,.6]])
    moved = state.copy().reshape(-1,3,2)@rotation.T
    moved[:,0] += [100.,-300.]
    assert np.allclose(hermite_controls(moved.reshape(-1,6)),controls@rotation.T+[100.,-300.],atol=1e-13,rtol=0)
    for node in range(1,len(state)-1):
        for field in range(6):
            altered = state.copy()
            altered[node,field] += .5
            affected = np.flatnonzero(np.any(np.abs(hermite_controls(altered)-controls)>1e-10,axis=(1,2)))
            assert set(affected)=={node-1,node}
    for count in (3,6,12,18):
        pattern = residual_dependencies(count)
        variables = 6*(count-2)+2
        assert pattern.shape==(93*(count-1)+variables,variables)
        assert pattern.nnz<=12*93*(count-1)+2*93+variables
    return dict(independent_hermite_endpoints=True,C2_joins=True,rigid_transform_invariant=True,
                neighboring_controls_only=True,jacobian_storage_linear=True)


def verify_fitting(vehicle):
    surface = shapely.box(-20,-4,20,4)
    route = shapely.LineString([[-8,0],[8,0]])
    entry,exit = [dict(position=np.array([x,0.]),heading=np.array([1.,0.])) for x in (-8,8)]
    path,note = fit_hermite_route(entry,exit,route,surface,vehicle,maximum_calls=1)
    assert path is not None and note['residual_calls']==1 and note['vehicle_sweep']['fits_surface']
    assert note['optimizer_status']=='optimizer_residual_budget_exhausted'
    for angle in (.3,1.7,3.9):
        matrix = np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
        transform = lambda points:points@matrix.T+[100.,-200.]
        poses = [dict(position=transform(p['position']),heading=p['heading']@matrix.T) for p in (entry,exit)]
        moved,note = fit_hermite_route(*poses,shapely.transform(route,transform),shapely.transform(surface,transform),
                                      vehicle,maximum_calls=1)
        assert moved is not None and note['residual_calls']==1
        assert np.allclose(shapely.get_coordinates(moved)[[0,-1]],[p['position'] for p in poses],atol=1e-10,rtol=0)
    split = shapely.union_all([shapely.box(-20,-4,-1,4),shapely.box(1,-4,20,4)])
    path,note = fitted_route_candidates(entry,exit,route,split,vehicle,maximum_calls=12)
    assert path is None and note['residual_calls']<=12
    path,note = fit_hermite_route(entry,exit,route,surface,vehicle,maximum_calls=1,maximum_poses=2)
    assert path is None and note['reason']=='spline_pose_budget_exhausted'
    return dict(actual_residual_budget=True,budgeted_valid_seed=True,rotated_translated_fits=True,
                disconnected_floor_rejected=True,continuous_body_budget=True)


def fit_module(network,module,maximum_calls):
    before,view = network.record(module)
    shape,surface,poses,paths,turns = view
    routes,_ = directed_module_routes(network.graph,poses,shape,network.tree)
    fitted_paths,fitted_turns = list(paths),list(turns)
    trials = []
    for index,turn in enumerate(turns):
        if turn['accepted'] or turn.get('reason')=='fixed_port_body_outside_surface':
            continue
        a,b = turn['entry'],turn['exit']
        started = time.perf_counter()
        path,note = fitted_route_candidates(poses[a],poses[b],routes[a,b],surface,network.vehicle,
                                           network.curbs['arc_error_m'],maximum_calls)
        milliseconds = (time.perf_counter()-started)*1000
        assert note['residual_calls']<=maximum_calls
        trials.append(dict(entry=a,exit=b,accepted=path is not None,solve_ms=milliseconds,**note))
        if path is not None:
            fitted_paths[index] = path
            fitted_turns[index] = dict(note,entry=a,exit=b,accepted=True,solve_ms=milliseconds)
    after = dict(before,turns=fitted_turns,accepted=sum(t['accepted'] for t in fitted_turns),
                 reasons=dict(Counter(t['reason'] for t in fitted_turns if not t['accepted'])))
    assert preserves_routes({module:before},{module:after})
    return (after,(shape,surface,poses,fitted_paths,fitted_turns)),trials


def survey(root,reference,vehicle,maximum_calls):
    started = time.perf_counter()
    usage = resource.getrusage(resource.RUSAGE_SELF)
    network = DrivingNetwork(root,reference['place'],vehicle)
    network.frames = {frame_key(f):{k:np.array(f[k]) for k in ('position','heading')} for f in reference['frames']}
    before,after,trials = {},{},[]
    views = []
    for index,original in enumerate(reference['modules']):
        module = original['module']
        before[module] = network.record(module)
        actual = before[module][0]
        assert accepted_routes(actual)==accepted_routes(original)
        assert {(t['entry'],t['exit']) for t in actual['turns']}=={(t['entry'],t['exit']) for t in original['turns']}
        after[module],local = fit_module(network,module,maximum_calls)
        trials.extend(dict(module=module,**row) for row in local)
        if accepted_routes(after[module][0])-accepted_routes(actual):
            views.append(((reference['place']+' before',module,*before[module]),
                          (reference['place']+' after',module,*after[module])))
        if (index+1)%32==0:
            print(json.dumps(dict(place=reference['place'],modules=index+1,trials=len(trials),
                             gained=sum(row['accepted'] for row in trials))),flush=True)
    counts = lambda records:dict(modules=len(records),connections=sum(r[0]['connections'] for r in records.values()),
                                accepted=sum(r[0]['accepted'] for r in records.values()))
    ended = resource.getrusage(resource.RUSAGE_SELF)
    inputs = {str(path.relative_to(root)):hashlib.sha256(path.read_bytes()).hexdigest() for path in
              [root/folder/f'{reference["place"]}-{suffix}.{extension}' for folder,suffix in
               [('directed-graphs','flat-network'),('directed-surfaces','flat-surfaces')] for extension in ('npz','json')]}
    report = dict(place=reference['place'],before=counts(before),after=counts(after),
                  trials=trials,modules=[after[k][0] for k in before],
                  total_ms=(time.perf_counter()-started)*1000,frames=reference['frames'],
                  input_sha256=inputs,
                  cpu_user_ms=(ended.ru_utime-usage.ru_utime)*1000,cpu_system_ms=(ended.ru_stime-usage.ru_stime)*1000,
                  process_peak_rss_bytes=ended.ru_maxrss*(1 if sys.platform=='darwin' else 1024))
    return report,views


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,default=Path('build/experiments/infrastructure'))
    parser.add_argument('--frames',type=Path,default=Path('build/experiments/infrastructure/pose-frames/shared-frames.json'))
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--shots',type=Path)
    parser.add_argument('--maximum-calls',type=int,default=1024)
    parser.add_argument('places',nargs='*')
    args = parser.parse_args()
    if args.maximum_calls<1:
        parser.error('residual budget must be positive')
    vehicle = json.loads(Path(__file__).with_name('infrastructure_network_recipes.json').read_text())['vehicleProfiles']['compact']
    checks = {**verify_clearance(),**verify_hermite(),**verify_fitting(vehicle)}
    reference = json.loads(args.frames.read_text())
    reports,views = [],[]
    for name in args.places:
        source = next(p for p in reference['places'] if p['place']==name)
        report,local = survey(args.input,source,vehicle,args.maximum_calls)
        reports.append(report)
        preferred = sorted(local,key=lambda v:(v[0][1] not in (8207,6958),v[0][1]))
        views.extend(preferred[:max(0,3-len(views))])
        print(json.dumps({k:v for k,v in report.items() if k not in ('trials','modules','frames','input_sha256')}),flush=True)
    args.output.mkdir(parents=True,exist_ok=True)
    names = ('infrastructure_hermite_route_fitting.py','infrastructure_surface_clearance.py','infrastructure_spline_routes.py','infrastructure_driving_network.py',
             'infrastructure_directed_routes.py','infrastructure_junction_turn_experiment.py','infrastructure_junction_surfaces.py',
             'infrastructure_port_lanes.py','infrastructure_corridor_experiment.py','infrastructure_route_turns.py',
             'infrastructure_route_fairing.py','infrastructure_shared_frames.py','infrastructure_junction_turns.py',
             'infrastructure_vehicle_sweep.py','infrastructure_clothoids.py','infrastructure_clothoid_turns.py',
             'infrastructure_curvature_paths.py','infrastructure_turn_paths.py','infrastructure_network_recipes.json')
    result = dict(checks=checks,places=reports,
        implementation_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
                              (Path(__file__),*(Path(__file__).with_name(n) for n in names))},
        shared_frames_sha256=hashlib.sha256(args.frames.read_bytes()).hexdigest(),
        scope='Frozen shared frames, existing requests and road surfaces; selected ground car routes only.')
    (args.output/'hermite-routes.json').write_text(json.dumps(result,indent=2)+'\n')
    if args.shots and views:
        render(args.shots,[v[0] for v in views]+[v[1] for v in views],
               'Fitted C2 curves: unchanged road surfaces and shared ports; complete vehicle checked')
    print(json.dumps(checks),flush=True)


if __name__=='__main__':
    main()
