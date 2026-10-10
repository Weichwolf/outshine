"""Check usable corridor remainders and an absorbed motor dead end."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely

from infrastructure_flat_cuts import sections_of
from infrastructure_flat_surfaces import solve
from infrastructure_flat_surface_network import render
from infrastructure_port_lanes import junction_interfaces,lane_poses,module_shape,module_face_index
from infrastructure_junction_surfaces import mesh_surface_index
from infrastructure_junction_turn_experiment import solve_module


def verify_remainders():
    counts = []
    for scale in (.1,1.,10.):
        for length,expected in ((7.5,0),(8.5,0),(10.999,0),(11.001,1),(20.,1)):
            model = dict(lines=np.array([shapely.LineString([[0,0],[length*scale,0]])]),
                         length=np.array([length*scale]),depth=np.array([[7.5*scale,0.]]),
                         half=np.array([1.75*scale]))
            sections = sections_of(model,.001*scale)
            assert len(sections[0])==expected
            counts.append(expected)
    return dict(cases=len(counts),width_derived_threshold=True,scale_invariant=True)


def verify_dead_end(vehicle):
    vertices = np.array([[-20.,0.],[0.,0.],[20.,0.],[0.,8.5]])
    edges = np.array([[0,1],[1,2],[1,3]])
    graph = dict(vertices=vertices,edges=edges,half_widths=np.full(3,1.75),
                 travel_public=np.ones((3,2),dtype=np.uint8))
    graph['lines'] = shapely.linestrings(vertices[edges])
    product,report = solve([graph],.001,'cuts',2.,.025)
    ports = junction_interfaces(product)
    assert len(ports)==1 and len(next(iter(ports.values())))==2
    assert report['cuts']['absorbed_subwidth_remainders']==1
    assert report['coverage_error_m2']<1e-7 and report['ports']['invalid_sections']==0
    assert shapely.covers(shapely.union_all(shapely.polygons(product['vertices'][product['indices']])),
                          shapely.Point(0,8.5)),'the dead end surface must remain present'
    module = next(iter(ports))
    shape = module_shape(product,module,module_face_index(product))
    tree = shapely.STRtree(graph['lines'])
    poses,unused = lane_poses(graph,ports[module],shape,vehicle,tree=tree)
    turns,_ = solve_module(module,shape,poses,unused,graph,tree,mesh_surface_index(product),vehicle,
                           dict(corner_radius_m=2.,arc_error_m=.025))
    assert not turns['invalid_body_ports'] and turns['connections']==2 and turns['accepted']==2
    return dict(source_surface_preserved=True,false_terminal_port_removed=True,
                through_routes_preserved=True,cuts=report['cuts']),product


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--shot',type=Path)
    args = parser.parse_args()
    configuration = Path(__file__).with_name('infrastructure_network_recipes.json')
    vehicle = json.loads(configuration.read_text())['vehicleProfiles']['compact']
    checks,product = verify_dead_end(vehicle)
    result = dict(remainders=verify_remainders(),dead_end=checks,
                  implementation_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
                    (Path(__file__),Path(__file__).with_name('infrastructure_flat_cuts.py'),configuration)})
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    if args.shot is not None:
        render(args.shot,'Terminal spur retained within its junction',product,None)
    print(json.dumps(result))


if __name__=='__main__':
    main()
