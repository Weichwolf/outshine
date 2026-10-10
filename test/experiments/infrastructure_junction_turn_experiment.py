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
from infrastructure_port_lanes import junction_interfaces,module_shape,lane_poses,module_face_index,align_vehicle_ports
from infrastructure_directed_routes import directed_module_routes
from infrastructure_junction_surfaces import mesh_surface_index,motor_surface,same_island_winding
from infrastructure_junction_turns import connect_lanes
from infrastructure_route_turns import source_route_connector
from infrastructure_building_clearance import plot_geometry
from infrastructure_travel import MODES
from infrastructure_vehicle_sweep import body_outlines


def verify_small(vehicle):
    graph = dict(vertices=np.array([[-10,0],[0,0],[10,0],[0,10]],dtype=float),
                 edges=np.array([[0,1],[1,2],[1,3]]),half_widths=np.full(3,3.),
                 travel_public=np.array([[1,0],[1,0],[4,4]],dtype=np.uint8))
    graph['lines'] = np.array([shapely.LineString(graph['vertices'][edge]) for edge in graph['edges']],dtype=object)
    ports = [dict(section=i,corridor=i,line=shapely.LineString(points)) for i,points in
             enumerate(([[-5,-3],[-5,3]],[[5,-3],[5,3]],[[-3,5],[3,5]]))]
    shape,tree = shapely.box(-5,-5,5,5),shapely.STRtree(graph['lines'])
    poses,unused = lane_poses(graph,ports,shape,vehicle,tree=tree)
    routes,_ = directed_module_routes(graph,poses,shape,tree)
    assert len(poses)==2 and not unused and list(routes)==[(0,1)]
    assert np.array_equal(shapely.get_coordinates(routes[0,1])[[0,-1]],np.array([[-5,0],[5,0]]))
    physical = np.array([shapely.box(-12,-3,12,3),shapely.box(-3,-3,3,12)],dtype=object)
    floor = motor_surface(shape,graph,tree,(physical,shapely.STRtree(physical)),vehicle)
    assert not shapely.covers(floor,shapely.Point(0,7)),'a walk-only branch is not a motor turning area'
    graph['travel_public'][2] = 0
    private_floor = motor_surface(shape,graph,tree,(physical,shapely.STRtree(physical)),vehicle)
    assert shapely.equals(floor,private_floor),'a private branch is not public turning area'
    return dict(directed_connection=True,walk_floor_excluded=True,private_floor_excluded=True)


def verify_one_way_route(vehicle):
    vertices = np.array([[-24,0],[-10,0],[-10,12],[10,12],[10,0],[24,0]],dtype=float)
    edges = np.array([[0,1],[1,2],[2,3],[3,4],[4,5],[4,1]])
    lines = np.array([shapely.LineString(vertices[e]) for e in edges],dtype=object)
    graph = dict(vertices=vertices,edges=edges,lines=lines,half_widths=np.full(6,4.),
                 travel_public=np.tile([1,0],(6,1)))
    entry = dict(position=np.array([-16.,0.]),heading=np.array([1.,0.]),edge=0,direction=1,
                 station=8.,section=0,incoming=True)
    exit = dict(position=np.array([16.,0.]),heading=np.array([1.,0.]),edge=4,direction=1,
                station=6.,section=1,incoming=False)
    tree = shapely.STRtree(lines)
    routes,_ = directed_module_routes(graph,[entry,exit],shapely.box(-16,-4,16,16),tree)
    floor = shapely.union_all(shapely.buffer(lines,4))
    assert connect_lanes(entry,exit,floor,vehicle)[0] is not None
    shortcut,_ = connect_lanes(entry,exit,floor,vehicle)
    assert not same_island_winding(shortcut,routes[0,1],floor),'reachability alone allows a counterflow shortcut'
    path,note = source_route_connector(entry,exit,routes[0,1],floor,vehicle)
    assert path is not None and note['vehicle_sweep']['fits_surface']
    assert same_island_winding(path,routes[0,1],floor)
    assert path.bounds[3]>8,'the legal route must use the upper one-way street'
    mesh = np.array([floor],dtype=object)
    report,view = solve_module(0,shapely.box(-16,-4,16,16),[entry,exit],[],graph,tree,
                             (mesh,shapely.STRtree(mesh)),vehicle,dict(corner_radius_m=2.,arc_error_m=.025))
    assert report['accepted']==1 and same_island_winding(view[3][0],routes[0,1],floor)
    for angle in (.3,1.8,3.7):
        rotation = np.array([[np.cos(angle),-np.sin(angle)],[np.sin(angle),np.cos(angle)]])
        transform = lambda xy:xy@rotation.T+np.array([100.,-70.])
        moved = [shapely.transform(g,transform) for g in (shortcut,path,routes[0,1],floor)]
        assert not same_island_winding(moved[0],moved[2],moved[3])
        assert same_island_winding(moved[1],moved[2],moved[3])
    return dict(counterflow_shortcut_rejected=True,legal_directed_route_preserved=True,
                route_guard_integrated=True,winding_rotation_invariant=True)


def selected_junctions(product,graph,tree,ports,faces,vehicle,limit):
    candidates = np.array(list(ports))
    first = faces[0][faces[1][candidates]]
    centres = product['vertices'][product['indices'][first]].mean(axis=1)
    accepted = 0
    for module in candidates[np.argsort(np.linalg.norm(centres,axis=1))]:
        shape = module_shape(product,module,faces)
        poses,unused = lane_poses(graph,ports[module],shape,vehicle,tree=tree)
        if sum(p['incoming'] for p in poses)<2 or sum(not p['incoming'] for p in poses)<2:
            continue
        if len(set(p['section'] for p in poses))<3:
            continue
        yield int(module),shape,poses,unused
        accepted += 1
        if accepted>=limit:
            break


def solve_module(module,shape,poses,unused,graph,tree,mesh,vehicle,curbs,*,adjust_ports=True):
    began = time.perf_counter()
    routes,excluded = directed_module_routes(graph,poses,shape,tree)
    surface = motor_surface(shape,graph,tree,mesh,vehicle,curbs['corner_radius_m'],curbs['arc_error_m'])
    poses,adjusted_ports = (align_vehicle_ports(poses,surface,vehicle,curbs['arc_error_m'])
                            if adjust_ports else (poses,[]))
    positions = np.array([p['position'] for p in poses])
    headings = np.array([np.arctan2(p['heading'][1],p['heading'][0]) for p in poses])
    bodies = shapely.polygons(body_outlines(positions,headings,vehicle['widthM']*.5,
                                          vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']))
    valid_ports = shapely.covers(surface,bodies)
    surface_ms = (time.perf_counter()-began)*1000
    local = tree.query(surface,predicate='intersects')
    flags = graph['travel_public'][local]&MODES['motor']
    has_one_way = bool(np.any(flags[:,0]!=flags[:,1]))
    results,paths,connectors = [],[],{}
    for entry,exit in routes:
        started = time.perf_counter()
        if not valid_ports[entry] or not valid_ports[exit]:
            results.append(dict(entry=entry,exit=exit,accepted=False,reason='fixed_port_body_outside_surface',
                                solve_ms=(time.perf_counter()-started)*1000))
            paths.append(None)
            continue
        path,report = connect_lanes(poses[entry],poses[exit],surface,vehicle,curbs['arc_error_m'])
        if path is not None and has_one_way and not same_island_winding(path,routes[entry,exit],surface):
            path,report = None,dict(reason='curve_changes_directed_island_winding')
        if path is None:
            trial,note = source_route_connector(poses[entry],poses[exit],routes[entry,exit],surface,vehicle,
                                                curbs['arc_error_m'],connectors=connectors)
            if trial is not None and has_one_way and not same_island_winding(trial,routes[entry,exit],surface):
                trial,note = None,dict(reason='curve_changes_directed_island_winding')
            if trial is not None:
                path,report = trial,note
            else:
                report['route_reason'] = note['reason']
        report.update(entry=entry,exit=exit,accepted=path is not None,solve_ms=(time.perf_counter()-started)*1000)
        results.append(report)
        paths.append(path)
    reasons = Counter(r['reason'] for r in results if not r['accepted'])
    report = dict(module=module,ports=len(set(p['section'] for p in poses)),connections=len(routes),
                  accepted=sum(r['accepted'] for r in results),reasons=dict(reasons),
                  excluded=excluded,unused_ports=unused,adjusted_ports=adjusted_ports,
                  invalid_body_ports=np.flatnonzero(~valid_ports).tolist(),
                  directed_island_winding=has_one_way,surface_ms=surface_ms,
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


def render(image_path,views,title=None):
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
        for path,turn in zip(paths,turns):
            if path is not None:
                ax.plot(*shapely.get_coordinates(path).T,color='#167dba',linewidth=1,alpha=.65)
            else:
                points = np.array([poses[turn['entry']]['position'],poses[turn['exit']]['position']])
                ax.plot(*points.T,color='#bd5744',linestyle=':',linewidth=.8)
        a,b,c,d = shape.bounds
        ax.set(xlim=(a-7,c+7),ylim=(b-7,d+7),aspect='equal',facecolor='#f1f0e8',
               xlabel='East [m]',ylabel='North [m]',title=f'{name} #{module}: {report["accepted"]}/{report["connections"]} clear routes')
    fig.suptitle(title or 'Actual public motor junctions: blue = full body checked; red dotted = unresolved request')
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
    checks = dict(**verify_small(vehicle),**verify_one_way_route(vehicle))
    results,selected = [],[]
    for name in args.places:
        report,views = survey(args.input,name,vehicle,args.modules)
        results.append(report)
        selected.extend(views[:2])
        print(json.dumps(dict(place=name,counts=report['counts'],total_ms=report['total_ms'])),flush=True)
    report = dict(vehicle_profile=vehicle,checks=checks,places=results,
                  scope='Selected actual level-zero junctions; complete windows, asymmetric curves and traffic control remain open.')
    names = ('infrastructure_port_lanes.py','infrastructure_flat_ports.py','infrastructure_directed_routes.py',
             'infrastructure_junction_surfaces.py','infrastructure_junction_turns.py','infrastructure_turn_paths.py',
             'infrastructure_vehicle_sweep.py','infrastructure_clothoid_turns.py','infrastructure_curvature_paths.py',
             'infrastructure_route_turns.py','infrastructure_route_fairing.py','infrastructure_spline_routes.py')
    report['implementation_sha256'] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
                                      (Path(__file__),configuration,*(Path(__file__).with_name(n) for n in names))}
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'turns.json').write_text(json.dumps(report,indent=2)+'\n')
    if args.shots is not None:
        render(args.shots/'actual-junction-turns.png',selected[:6])


if __name__=='__main__':
    main()
