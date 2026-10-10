#!/usr/bin/env python3
"""Measure actual directed junction turns; retain only selected presentation images."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely

from infrastructure_flat_surfaces import load_graphs
from infrastructure_port_lanes import junction_interfaces,junction_shape,lane_poses,module_face_index
from infrastructure_junction_routes import allowed_connections
from infrastructure_junction_surfaces import mesh_surface_index,motor_surface
from infrastructure_junction_turns import connect_lanes
from infrastructure_building_clearance import plot_geometry


def verify_small(vehicle):
    graph = dict(vertices=np.array([[-10,0],[0,0],[10,0],[0,10]],dtype=float),
                 edges=np.array([[0,1],[1,2],[1,3]]),half_widths=np.full(3,3.),
                 travel_public=np.array([[1,0],[1,0],[4,4]],dtype=np.uint8))
    graph['lines'] = np.array([shapely.LineString(graph['vertices'][edge]) for edge in graph['edges']],dtype=object)
    ports = [dict(section=i,corridor=i,line=shapely.LineString(points)) for i,points in
             enumerate(([[-5,-3],[-5,3]],[[5,-3],[5,3]],[[-3,5],[3,5]]))]
    shape,tree = shapely.box(-5,-5,5,5),shapely.STRtree(graph['lines'])
    poses,unused = lane_poses(graph,ports,shape,vehicle,tree=tree)
    connections,_ = allowed_connections(graph,poses,shape,tree)
    assert len(poses)==2 and not unused and connections==[(0,1)]
    physical = np.array([shapely.box(-12,-3,12,3),shapely.box(-3,-3,3,12)],dtype=object)
    floor = motor_surface(shape,graph,tree,(physical,shapely.STRtree(physical)),vehicle)
    assert not shapely.covers(floor,shapely.Point(0,7)),'a walk-only branch is not a motor turning area'
    graph['travel_public'][2] = 0
    private_floor = motor_surface(shape,graph,tree,(physical,shapely.STRtree(physical)),vehicle)
    assert shapely.equals(floor,private_floor),'a private branch is not public turning area'
    return dict(directed_connection=True,walk_floor_excluded=True,private_floor_excluded=True)


def selected_junctions(product,graph,tree,ports,faces,vehicle,limit):
    candidates = np.array(list(ports))
    first = faces[0][faces[1][candidates]]
    centres = product['vertices'][product['indices'][first]].mean(axis=1)
    accepted = 0
    for module in candidates[np.argsort(np.linalg.norm(centres,axis=1))]:
        shape = junction_shape(product,module,faces)
        poses,unused = lane_poses(graph,ports[module],shape,vehicle,tree=tree)
        if sum(p['incoming'] for p in poses)<2 or sum(not p['incoming'] for p in poses)<2:
            continue
        if len(set(p['section'] for p in poses))<3:
            continue
        yield int(module),shape,poses,unused
        accepted += 1
        if accepted>=limit:
            break


def solve_module(module,shape,poses,unused,graph,tree,mesh,vehicle,curbs):
    began = time.perf_counter()
    connections,excluded = allowed_connections(graph,poses,shape,tree)
    surface = motor_surface(shape,graph,tree,mesh,vehicle,curbs['corner_radius_m'],curbs['arc_error_m'])
    surface_ms = (time.perf_counter()-began)*1000
    results,paths = [],[]
    for entry,exit in connections:
        started = time.perf_counter()
        path,report = connect_lanes(poses[entry],poses[exit],surface,vehicle,curbs['arc_error_m'])
        report.update(entry=entry,exit=exit,accepted=path is not None,solve_ms=(time.perf_counter()-started)*1000)
        results.append(report)
        paths.append(path)
    reasons = Counter(r['reason'] for r in results if not r['accepted'])
    report = dict(module=module,ports=len(set(p['section'] for p in poses)),connections=len(connections),
                  accepted=sum(r['accepted'] for r in results),reasons=dict(reasons),
                  excluded=excluded,unused_ports=unused,surface_ms=surface_ms,
                  total_ms=(time.perf_counter()-began)*1000,turns=results)
    return report,(shape,surface,poses,paths,results)


def survey(root,name,vehicle,limit):
    began = time.perf_counter()
    graphs = load_graphs(root/'directed-graphs'/f'{name}-flat-network.npz',
                        json.loads((root/'directed-graphs'/f'{name}-flat-network.json').read_text()))
    graph = next(g for g in graphs if g['tier']==['land',0,'ground'])
    tree = shapely.STRtree(graph['lines'])
    source = root/'directed-surfaces'/f'{name}-flat-surfaces.npz'
    with np.load(source) as archive:
        product = {k:archive[k] for k in archive.files}
    surface_report = json.loads(source.with_suffix('.json').read_text())
    ports,faces,mesh = junction_interfaces(product),module_face_index(product),mesh_surface_index(product)
    results,views = [],[]
    for module,shape,poses,unused in selected_junctions(product,graph,tree,ports,faces,vehicle,limit):
        report,view = solve_module(module,shape,poses,unused,graph,tree,mesh,vehicle,surface_report['curbs'])
        results.append(report)
        views.append((name,module,report,view))
    counts = Counter()
    for r in results:
        counts.update(modules=1,connections=r['connections'],accepted=r['accepted'])
        counts.update(r['reasons'])
    return dict(place=name,counts=dict(counts),modules=results,total_ms=(time.perf_counter()-began)*1000),views


def render(image_path,views):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes = plt.subplots(2,3,figsize=(16,10),constrained_layout=True)
    for ax,(name,module,report,view) in zip(axes.ravel(),views):
        shape,surface,poses,paths,turns = view
        plot_geometry(ax,[surface],'#c7cdc3')
        plot_geometry(ax,[shape],'#73988855')
        for pose in poses:
            p,h = pose['position'],pose['heading']
            ax.arrow(*p,*(h*2),width=.07,head_width=.6,length_includes_head=True,color='#465f57')
        shown = Counter()
        for path,turn in zip(paths,turns):
            category = 'accepted' if path is not None else 'rejected'
            if shown[category]>=4:
                continue
            shown[category] += 1
            if path is not None:
                ax.plot(*shapely.get_coordinates(path).T,color='#167dba',linewidth=1)
            else:
                points = np.array([poses[turn['entry']]['position'],poses[turn['exit']]['position']])
                ax.plot(*points.T,color='#bd5744',linestyle=':',linewidth=.8)
        a,b,c,d = shape.bounds
        ax.set(xlim=(a-7,c+7),ylim=(b-7,d+7),aspect='equal',facecolor='#f1f0e8',
               xlabel='East [m]',ylabel='North [m]',title=f'{name} #{module}: {report["accepted"]}/{report["connections"]} clear turns')
    fig.suptitle('Actual public motor junctions: blue = full body checked; red dotted = unresolved request')
    image_path.parent.mkdir(parents=True,exist_ok=True)
    fig.savefig(image_path,dpi=140)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--shots',type=Path)
    parser.add_argument('--modules',type=int,default=12)
    parser.add_argument('places',nargs='+')
    args = parser.parse_args()
    if args.modules<1:
        parser.error('modules must be positive')
    configuration = Path(__file__).with_name('infrastructure_network_recipes.json')
    vehicle = json.loads(configuration.read_text())['vehicleProfiles']['compact']
    checks = verify_small(vehicle)
    results,selected = [],[]
    for name in args.places:
        report,views = survey(args.input,name,vehicle,args.modules)
        results.append(report)
        selected.extend(views[:2])
        print(json.dumps(dict(place=name,counts=report['counts'],total_ms=report['total_ms'])),flush=True)
    report = dict(vehicle_profile=vehicle,checks=checks,places=results,
                  scope='Selected actual level-zero junctions; complete windows, asymmetric curves and traffic control remain open.')
    names = ('infrastructure_port_lanes.py','infrastructure_flat_ports.py','infrastructure_junction_routes.py',
             'infrastructure_junction_surfaces.py','infrastructure_junction_turns.py','infrastructure_turn_paths.py',
             'infrastructure_vehicle_sweep.py','infrastructure_clothoid_turns.py','infrastructure_curvature_paths.py')
    report['implementation_sha256'] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
                                      (Path(__file__),configuration,*(Path(__file__).with_name(n) for n in names))}
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'turns.json').write_text(json.dumps(report,indent=2)+'\n')
    if args.shots is not None:
        render(args.shots/'actual-junction-turns.png',selected[:6])


if __name__=='__main__':
    main()
