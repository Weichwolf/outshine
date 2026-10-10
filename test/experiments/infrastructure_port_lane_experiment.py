#!/usr/bin/env python3
"""Check source-permitted lane poses at the connected shared module ports."""

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


def verify_small(vehicle):
    product = dict(vertices=np.array([[-5,-3],[-5,3],[5,-3],[5,3]],dtype=float),
                   port_edges=np.array([[0,1],[2,3]]),port_parts=np.array([[0,1],[0,1]]),
                   part_roles=np.array([1,0]))
    ports = junction_interfaces(product)[0]
    assert len(ports)==2,'disjoint interfaces between the same parts must remain separate'
    graph = dict(lines=np.array([shapely.LineString([[-10,0],[10,0]])],dtype=object),
                 half_widths=np.array([3.]),travel_public=np.array([[1,1]],dtype=np.uint8))
    shape = shapely.box(-5,-3,5,3)
    for side in (-1,1):
        poses,unused = lane_poses(graph,ports,shape,vehicle,traffic_side=side)
        assert len(poses)==4 and not unused
        assert sum(p['incoming'] for p in poses)==2
        for pose in poses:
            assert np.isclose(pose['position'][1],-1.5*side*pose['heading'][0])
            assert pose['incoming']==(pose['position'][0]*pose['heading'][0]<0)
    graph['half_widths'][:] = 1.5
    poses,unused = lane_poses(graph,ports,shape,vehicle)
    assert len(poses)==4 and not unused and all(p['shared_lane'] for p in poses)
    assert all(p['position'][1]==0 for p in poses)
    graph['travel_public'][:] = [0,1]
    poses,unused = lane_poses(graph,ports,shape,vehicle)
    assert len(poses)==2 and not unused and all(p['heading'][0]==-1 for p in poses)
    graph['travel_public'][:] = 0
    assert lane_poses(graph,ports,shape,vehicle)==([],[])
    graph['travel_public'][:] = 1
    graph['lines'][0] = shapely.LineString([[-10,-1],[0,-1],[0,1],[-10,1]])
    poses,unused = lane_poses(graph,ports,shape,vehicle)
    assert len(poses)==4 and not unused,'one folded axis can cross a section twice'
    graph['lines'][0] = shapely.LineString([[-10,0],[10,0]])
    narrow = shapely.box(-.003,-3,.003,3)
    close_ports = [dict(port,line=shapely.LineString([[x,-3],[x,3]])) for port,x in zip(ports,(-.003,.003))]
    poses,unused = lane_poses(graph,close_ports,narrow,vehicle)
    assert len(poses)==4 and not unused,'interior probing must adapt to nearby sections'
    assert all(p['incoming']==(p['position'][0]*p['heading'][0]<0) for p in poses)
    return dict(disjoint_loop_ports=True,traffic_sides=True,shared_narrow_lane=True,
                reverse_one_way=True,mode_access=True,repeated_axis_crossing=True,nearby_sections=True)


def survey(root,name,vehicle):
    began = time.perf_counter()
    graphs = load_graphs(root/'directed-graphs'/f'{name}-flat-network.npz',
                        json.loads((root/'directed-graphs'/f'{name}-flat-network.json').read_text()))
    graph = next(g for g in graphs if g['tier']==['land',0,'ground'])
    tree = shapely.STRtree(graph['lines'])
    with np.load(root/'directed-surfaces'/f'{name}-flat-surfaces.npz') as archive:
        product = {k:archive[k] for k in archive.files}
    interfaces = junction_interfaces(product)
    faces = module_face_index(product)
    prepared_ms = (time.perf_counter()-began)*1000
    counts,reasons,unused_ports = Counter(),Counter(),[]
    for junction,ports in interfaces.items():
        poses,unused = lane_poses(graph,ports,junction_shape(product,junction,faces),vehicle,tree=tree)
        counts.update(modules=1,incoming=sum(p['incoming'] for p in poses),
                      outgoing=sum(not p['incoming'] for p in poses),shared_lane=sum(p['shared_lane'] for p in poses),
                      motor_modules=bool(poses),omitted=len(unused))
        reasons.update(u['reason'] for u in unused)
        unused_ports.extend(dict(module=junction,**u) for u in unused)
    return dict(place=name,sections=sum(map(len,interfaces.values())),counts=dict(counts),
                omission_reasons=dict(reasons),unused=unused_ports,prepared_ms=prepared_ms,
                total_ms=(time.perf_counter()-began)*1000)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('places',nargs='+')
    args = parser.parse_args()
    configuration = Path(__file__).with_name('infrastructure_network_recipes.json')
    vehicle = json.loads(configuration.read_text())['vehicleProfiles']['compact']
    checks = verify_small(vehicle)
    results = [survey(args.input,name,vehicle) for name in args.places]
    report = dict(checks=checks,vehicle_profile=vehicle,places=results,
                  scope='Source-permitted port lane poses; complete junction traffic and driveable turns remain open.')
    report['implementation_sha256'] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
        (Path(__file__),configuration,*(Path(__file__).with_name(n) for n in
          ('infrastructure_flat_ports.py','infrastructure_port_lanes.py','infrastructure_flat_surfaces.py')))}
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'ports.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps([dict(place=r['place'],sections=r['sections'],counts=r['counts'],
                          omission_reasons=r['omission_reasons'],total_ms=r['total_ms']) for r in results]))


if __name__=='__main__':
    main()
