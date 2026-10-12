"""Join junction-owned lane ports through actual shared corridor modules."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely

from transport_flat_surfaces import load_graphs
from transport_port_lanes import junction_interfaces,module_shape,lane_poses,module_face_index,align_vehicle_ports
from transport_junction_surfaces import mesh_surface_index,motor_surface
from transport_junction_turn_experiment import solve_module,render


def corridor_ports(interfaces,registry):
    poses = []
    for junction,section in interfaces:
        poses.extend(dict(p,incoming=not p['incoming'],junction=junction)
                     for p in registry[junction] if p['section']==section)
    return poses


def verify_shared_ports():
    original = dict(position=np.array([1.,2.]),heading=np.array([0.,1.]),incoming=False,
                    section=17,edge=3,direction=-1,station=2.)
    poses = corridor_ports([(4,17)],{4:[original]})
    assert len(poses)==1 and poses[0]['incoming'] and not original['incoming']
    assert poses[0]['position'] is original['position'] and poses[0]['heading'] is original['heading']
    assert all(poses[0][k]==original[k] for k in ('section','edge','direction','station'))
    assert corridor_ports([(4,18)],{4:[original]})==[]
    return dict(shared_position_and_heading=True,source_direction_preserved=True,opposite_module_entry=True)


def verify_invalid_endpoint(vehicle):
    line = shapely.LineString([[-10,0],[10,0]])
    graph = dict(lines=np.array([line],dtype=object),vertices=np.array([[-10.,0.],[10.,0.]]),
                 edges=np.array([[0,1]]),half_widths=np.array([2.]),travel_public=np.array([[1,1]]))
    poses = [dict(position=np.array([x,y]),heading=np.array([1.,0.]),incoming=i==0,
                  edge=0,direction=1,station=x+10,section=i) for i,(x,y) in enumerate(((-5.,0.),(5.,1.5)))]
    physical = np.array([shapely.box(-10,-2,10,2)],dtype=object)
    report,_ = solve_module(0,shapely.box(-5,-2,5,2),poses,[],graph,shapely.STRtree(graph['lines']),
                           (physical,shapely.STRtree(physical)),vehicle,
                           dict(corner_radius_m=2.,arc_error_m=.025),adjust_ports=False)
    assert report['invalid_body_ports']==[1] and report['connections']==1 and report['accepted']==0
    assert report['turns'][0]['reason']=='fixed_port_body_outside_surface'
    return dict(invalid_fixed_endpoint_rejected=True)


def survey(root,name,vehicle,limit):
    began = time.perf_counter()
    graphs = load_graphs(root/'directed-graphs'/f'{name}-flat-network.npz',
                        json.loads((root/'directed-graphs'/f'{name}-flat-network.json').read_text()))
    graph = next(g for g in graphs if g['tier']==['land',0,'ground'])
    tree = shapely.STRtree(graph['lines'])
    source = root/'directed-surfaces'/f'{name}-flat-surfaces.npz'
    with np.load(source) as archive:
        product = {k:archive[k] for k in archive.files}
    curbs = json.loads(source.with_suffix('.json').read_text())['curbs']
    junctions,faces,mesh = junction_interfaces(product),module_face_index(product),mesh_surface_index(product)
    corridors = {}
    for junction,ports in junctions.items():
        for port in ports:
            corridors.setdefault(port['corridor'],[]).append((junction,port['section']))
    candidates = np.array([k for k,v in corridors.items() if len(v)>1],dtype=int)
    first = faces[0][faces[1][candidates]]
    centres = product['vertices'][product['indices'][first]].mean(axis=1)
    registry,results,views,excluded = {},[],[],[]
    for corridor in candidates[np.argsort(np.linalg.norm(centres,axis=1))]:
        interfaces = corridors[corridor]
        for junction,_ in interfaces:
            if junction in registry:
                continue
            shape = module_shape(product,junction,faces)
            poses,_ = lane_poses(graph,junctions[junction],shape,vehicle,tree=tree)
            if poses:
                surface = motor_surface(shape,graph,tree,mesh,vehicle,curbs['corner_radius_m'],curbs['arc_error_m'])
                poses,_ = align_vehicle_ports(poses,surface,vehicle,curbs['arc_error_m'])
            registry[junction] = poses
        poses = corridor_ports(interfaces,registry)
        reason = ('no_public_motor_ports' if not poses else
                  'single_public_motor_section' if len({p['section'] for p in poses})<2 else
                  'no_opposed_motor_ports' if not any(p['incoming'] for p in poses) or
                  not any(not p['incoming'] for p in poses) else None)
        if reason is not None:
            excluded.append(dict(module=int(corridor),reason=reason))
            continue
        shape = module_shape(product,corridor,faces)
        report,view = solve_module(int(corridor),shape,poses,[],graph,tree,mesh,vehicle,curbs,adjust_ports=False)
        assert not report['adjusted_ports']
        report['junctions'] = sorted(set(j for j,_ in interfaces))
        results.append(report)
        views.append((name,int(corridor),report,view))
        if len(results)>=limit:
            break
    counts = Counter()
    for result in results:
        counts.update(modules=1,connections=result['connections'],accepted=result['accepted'])
        counts.update(result['reasons'])
    return dict(place=name,counts=dict(counts),modules=results,excluded_modules=excluded,junctions_prepared=len(registry),
                total_ms=(time.perf_counter()-began)*1000),views


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--shots',type=Path)
    parser.add_argument('--modules',type=int,default=24)
    parser.add_argument('places',nargs='+')
    args = parser.parse_args()
    if args.modules<1:
        parser.error('modules must be positive')
    configuration = Path(__file__).with_name('transport_network_recipes.json')
    vehicle = json.loads(configuration.read_text())['vehicleProfiles']['compact']
    reports,views = [],[]
    for name in args.places:
        report,local = survey(args.input,name,vehicle,args.modules)
        reports.append(report)
        ranked = sorted(local,key=lambda v:(not any('fairing_weight' in t for t in v[2]['turns']),
                                            v[2]['accepted']==v[2]['connections'],v[1]))
        views.extend(ranked[:2])
        print(json.dumps(dict(place=name,counts=report['counts'],total_ms=report['total_ms'])),flush=True)
    names = ('transport_directed_routes.py','transport_port_lanes.py','transport_junction_turn_experiment.py',
             'transport_route_turns.py','transport_route_fairing.py','transport_spline_routes.py','transport_vehicle_sweep.py',
             'transport_clothoids.py','transport_clothoid_turns.py','transport_curvature_paths.py',
             'transport_junction_surfaces.py','transport_junction_turns.py','transport_turn_paths.py')
    report = dict(checks=dict(**verify_shared_ports(),**verify_invalid_endpoint(vehicle)),places=reports,vehicle_profile=vehicle,
                  scope='Corridors use fixed neighboring junction poses; complete network and traffic conflicts remain open.',
                  implementation_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
                    (Path(__file__),configuration,*(Path(__file__).with_name(n) for n in names))})
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'corridors.json').write_text(json.dumps(report,indent=2)+'\n')
    if args.shots is not None:
        render(args.shots/'shared-corridor-routes.png',views[:6],
               'Corridor routes: fixed neighboring junction ports; blue = body checked; red dotted = unresolved')


if __name__=='__main__':
    main()
