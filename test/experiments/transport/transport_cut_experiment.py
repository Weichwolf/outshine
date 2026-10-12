"""Check usable corridor remainders and an absorbed motor dead end."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely

from transport_flat_cuts import sections_of
from transport_flat_surfaces import solve
from transport_flat_surface_network import render
from transport_port_lanes import junction_interfaces,lane_poses,module_shape,module_face_index
from transport_junction_surfaces import mesh_surface_index,motor_surface
from transport_junction_turn_experiment import solve_module
from transport_cut_clearance import motor_model,invalid_vehicle_sections
from transport_vehicle_sweep import body_outlines


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


def verify_no_motor_contacts(vehicle):
    vertices=np.array([[0.,0.],[10.,0.]])
    graph=dict(vertices=vertices,edges=np.array([[0,1]]),half_widths=np.array([2.]),
               travel_public=np.zeros((1,2),dtype=np.uint8))
    graph['lines']=shapely.linestrings(vertices[graph['edges']]);tree=shapely.STRtree(graph['lines'])
    product=dict(vertices=np.array([[-2.,-2.],[12.,-2.],[-2.,2.],[12.,2.]]),indices=np.array([[0,1,2],[1,3,2]]))
    unused=[dict(section=7,reason='no_public_motor_access')];shape=shapely.box(-2,-2,12,2)
    report,state=solve_module(0,shape,[],unused,graph,tree,mesh_surface_index(product),vehicle,
                             dict(corner_radius_m=2.,arc_error_m=.025))
    assert report['connections']==report['accepted']==0 and not report['invalid_body_ports']
    assert report['unused_ports']==unused and state[0].equals(shape)
    return dict(no_phantom_motor_routes=True,source_geometry_retained=True,unused_contacts_retained=True)


def verify_dead_end(vehicle):
    vertices = np.array([[-20.,0.],[0.,0.],[20.,0.],[0.,8.5]])
    edges = np.array([[0,1],[1,2],[1,3]])
    graph = dict(vertices=vertices,edges=edges,half_widths=np.full(3,1.75),
                 travel_public=np.ones((3,2),dtype=np.uint8))
    graph['lines'] = shapely.linestrings(vertices[edges])
    product,report = solve([graph],.001,'cuts',2.,.025,vehicle=vehicle)
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


def invalid_port_bodies(product,graph,vehicle):
    tree,faces,mesh = shapely.STRtree(graph['lines']),module_face_index(product),mesh_surface_index(product)
    invalid = []
    for module,ports in junction_interfaces(product).items():
        shape = module_shape(product,module,faces)
        poses,unused = lane_poses(graph,ports,shape,vehicle,tree=tree)
        if not poses:
            continue
        surface = motor_surface(shape,graph,tree,mesh,vehicle)
        positions = np.array([p['position'] for p in poses])
        angles = np.array([np.arctan2(p['heading'][1],p['heading'][0]) for p in poses])
        bodies = shapely.polygons(body_outlines(positions,angles,vehicle['widthM']*.5,
                                  vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']))
        invalid.extend((module,int(i)) for i in np.flatnonzero(~shapely.covers(surface,bodies)))
    return invalid


def verify_foreign_cut(vehicle):
    vertices = np.array([[0.,0.],[24.,0.],[10.,1.5],[18.,1.5],[40.,1.5],[18.,12.]])
    edges = np.array([[0,1],[2,3],[3,4],[3,5]])
    graph = dict(vertices=vertices,edges=edges,lines=shapely.linestrings(vertices[edges]),
                 half_widths=np.array([1.75,.9,.9,.9]),
                 travel_public=np.array([[23,23],[4,4],[4,4],[4,4]],dtype=np.uint8))
    original,before = solve([graph],.001,'cuts',2.,.025)
    corrected,after = solve([graph],.001,'cuts',2.,.025,vehicle=vehicle)
    assert invalid_port_bodies(original,graph,vehicle)
    assert not invalid_port_bodies(corrected,graph,vehicle)
    assert before['area_m2']==after['area_m2'] and after['coverage_error_m2']<1e-7
    assert after['cuts']['vehicle_cut_adjustments']>0 and after['cuts']['vehicle_poses_checked']>0
    assert after['ports']['invalid_sections']==0 and not after['cuts']['omitted_cuts']
    return dict(foreign_mode_cut_repaired=True,source_surface_preserved=True,
                all_fixed_motor_ports_valid=True,cuts=after['cuts'])


def verify_directional_clearance(vehicle):
    line = shapely.LineString([[-10,0],[10,0]])
    graph = dict(lines=np.array([line],dtype=object),half_widths=np.array([1.75]),
                 travel_public=np.array([[1,0]],dtype=np.uint8))
    surface = shapely.buffer(line,1.75,quad_segs=2)
    span = np.array([[[9.8,-1.75],[9.8,1.75]]])
    model = motor_model([graph],surface,vehicle,0.,.025)
    assert invalid_vehicle_sections(span,model,vehicle,.025)[0].tolist()==[0]
    graph['travel_public'] = np.array([[0,1]],dtype=np.uint8)
    model = motor_model([graph],surface,vehicle,0.,.025)
    assert not len(invalid_vehicle_sections(span,model,vehicle,.025)[0])
    graph['travel_public'][:] = 0
    model = motor_model([graph],surface,vehicle,0.,.025)
    assert not len(model['lines']) and not len(invalid_vehicle_sections(span,model,vehicle,.025)[0])
    return dict(source_direction_enforced=True,non_motor_axes_excluded=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--shot',type=Path)
    args = parser.parse_args()
    configuration = Path(__file__).with_name('transport_network_recipes.json')
    vehicle = json.loads(configuration.read_text())['vehicleProfiles']['compact']
    checks,product = verify_dead_end(vehicle)
    result = dict(remainders=verify_remainders(),dead_end=checks,no_motor_contacts=verify_no_motor_contacts(vehicle),
                  foreign_cut=verify_foreign_cut(vehicle),directional=verify_directional_clearance(vehicle),
                  implementation_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
                    (Path(__file__),Path(__file__).with_name('transport_flat_cuts.py'),
                     Path(__file__).with_name('transport_cut_clearance.py'),configuration)})
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    if args.shot is not None:
        render(args.shot,'Terminal spur retained within its junction',product,None)
    print(json.dumps(result))


if __name__=='__main__':
    main()
