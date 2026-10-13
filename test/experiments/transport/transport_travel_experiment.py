#!/usr/bin/env python3
"""Check transport permissions on small counterexamples and cached real windows."""

import argparse
import hashlib
import json
from pathlib import Path
import time
from tempfile import TemporaryDirectory

import numpy as np
import shapely
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import shortest_path

from transport_flat_plan import plan
from transport_network import transport_widths
from transport_flat_surfaces import load_graphs
from transport_travel import MODES,MOTOR_CLASSES,bound_source_travel,compile_travel,source_travel


def verify(recipes):
    def road(xy,**tags):
        return dict(line=shapely.LineString(xy),width=4.,properties=dict({'class':'minor'},**tags))
    source = [road([(-10,0),(10,0)],oneway=1),
              road([(0,0),(0,10)],**{'class':'path','subclass':'steps','access':'yes'})]
    roads,graphs,_ = plan(source,recipes,{})
    travel = compile_travel(roads,graphs,recipes['travelRecipes'])
    graph = graphs[('land',0,'ground')]
    selected = graph['travel_public']&MODES['motor']!=0
    pairs = np.r_[graph['edges'][selected[:,0]],graph['edges'][selected[:,1]][:,::-1]]
    network = coo_matrix((np.ones(len(pairs)),pairs.T),shape=(len(graph['vertices']),)*2).tocsr()
    distance = shortest_path(network,directed=True)
    west,east,north = [int(np.argmin(np.linalg.norm(graph['vertices']-p,axis=1)))
                       for p in ((-10,0),(10,0),(0,10))]
    assert np.isfinite(distance[west,east]) and not np.isfinite(distance[east,west])
    assert not np.isfinite(distance[west,north])
    samples = [road([(0,0),(10,0)],access='no',foot='yes'),
               road([(0,0),(10,0)],oneway=1,**{'oneway:bicycle':'no'}),
               road([(0,0),(10,0)],oneway=-1),
               road([(0,0),(10,0)],access='private',foot='yes'),
               road([(0,0),(10,0)],bicycle='dismount'),
               road([(0,0),(10,0)],foot='use_sidepath'),
               road([(0,0),(10,0)],horse='o')]
    public,restricted,notes = source_travel(samples,recipes['travelRecipes'])
    assert public[0].tolist()==[MODES['walk']]*2
    assert public[1,0]&MODES['motor'] and not public[1,1]&MODES['motor']
    assert np.all(public[1]&MODES['cycle'])
    assert not public[2,0]&MODES['motor'] and public[2,1]&MODES['motor']
    assert public[3].tolist()==[MODES['walk']]*2 and np.all(restricted[3]&MODES['motor'])
    assert not np.any(public[4]&MODES['cycle']) and np.all(public[4]&MODES['walk'])
    assert not np.any(public[5]&MODES['walk'])
    assert not np.any(public[6]&MODES['horse'])
    assert {n['reason'] for n in notes}=={'dismount','use_sidepath','unresolved_access'}
    return dict(oneway_route=True,motor_route_into_steps=False,source_cases=len(samples),
                public_directed_arcs=travel['public_directed_arcs'])


def verify_motor_classes(recipes):
    roads=[dict(properties={'class':'service','hgv':'no'}),
           dict(properties={'class':'service','motorcar':'no','hgv':'delivery'}),
           dict(properties={'class':'busway'}),
           dict(properties={'class':'busway','motorcar':'yes'}),
           dict(properties={'class':'service','oneway':'yes','oneway:hgv':'-1'})]
    profiles={name:source_travel(roads,recipes['travelRecipes'],motor_class=name)
              for name in ('motorcar','hgv','bus')}
    car,car_restricted,_=profiles['motorcar'];truck,truck_restricted,_=profiles['hgv'];bus,_,_=profiles['bus']
    motor=MODES['motor']
    assert np.all(car[0]&motor) and not np.any(truck[0]&motor)
    assert not np.any(car[1]&motor) and not np.any(car_restricted[1]&motor)
    assert not np.any(truck[1]&motor) and np.all(truck_restricted[1]&motor)
    assert not np.any(car[2]&motor) and not np.any(truck[2]&motor) and np.all(bus[2]&motor)
    assert np.all(car[3]&motor)
    assert car[4,0]&motor and not car[4,1]&motor
    assert not truck[4,0]&motor and truck[4,1]&motor
    try:
        source_travel(roads,recipes['travelRecipes'],motor_class='unknown')
    except ValueError:
        pass
    else:
        raise AssertionError('unknown vehicle class accepted')
    return dict(motor_vehicle_classes=list(profiles),class_permissions_separate=True,
                class_directions_separate=True,busway_requires_bus=True)


def verify_cached_vehicle_classes(recipes):
    roads=[dict(properties={'class':'busway','oneway':1}),
           dict(properties={'class':'service','oneway':1,'hgv':'no'})]
    usage=[dict(id='busway',source=0),dict(id='service',source=1)]
    vertices=np.array([[0.,0.],[10.,0.],[10.,10.]])
    arrays=dict(source_widths=np.array([4.,4.]),**{'0_vertices':vertices,
        '0_edges':np.array([[0,1],[1,2]]),'0_line_coordinates':vertices[[0,1,1,2]],
        '0_line_offsets':np.array([0,2,4]),'0_owner_sources':np.array([0,1]),
        '0_owner_offsets':np.array([0,1,2]),'0_owner_directions':np.array([-1,1]),
        '0_travel_public':np.full((2,2),255,dtype=np.uint8),
        '0_travel_restricted':np.zeros((2,2),dtype=np.uint8)})
    with TemporaryDirectory(prefix='outshine-vehicle-access-') as directory:
        path=Path(directory)/'graph.npz';np.savez_compressed(path,**arrays)
        for name,expected in (('motorcar',[[False,True],[False,False]]),
                              ('bus',[[False,True],[True,False]]),
                              ('hgv',[[False,False],[False,False]])):
            public,restricted,_=bound_source_travel(roads,['service','busway'],usage,
                                                    recipes['travelRecipes'],motor_class=name)
            graph=load_graphs(path,dict(graphs=[dict(tier=['land',0,'ground'])]),
                              travel_permissions=(public,restricted))[0]
            assert np.array_equal(graph['vertices'],vertices)
            assert np.array_equal(graph['travel_public']&MODES['motor']!=0,expected)
    try:
        bound_source_travel(roads,['missing'],usage,recipes['travelRecipes'])
    except AssertionError:
        pass
    else:
        raise AssertionError('unbound graph source accepted')
    return dict(cached_geometry_unchanged=True,cached_access_recompiled=True,
                source_order_and_reversed_owner_preserved=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--motor-class',choices=sorted(MOTOR_CLASSES),default='motorcar')
    parser.add_argument('places',nargs='*')
    args = parser.parse_args()
    if args.places and args.input is None:
        parser.error('places require --input')
    args.output.mkdir(parents=True,exist_ok=True)
    configuration = Path(__file__).with_name('transport_network_recipes.json')
    recipes = json.loads(configuration.read_text())
    implementation = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
                      (Path(__file__),Path(__file__).with_name('transport_travel.py'),configuration)}
    checks=verify(recipes)|verify_motor_classes(recipes)|verify_cached_vehicle_classes(recipes)
    (args.output/'small.json').write_text(json.dumps(checks,indent=2)+'\n')
    widths = transport_widths()
    for place in args.places:
        path = args.input/f'{place}-input.json'
        raw = json.loads(path.read_text())
        transport = [dict(line=shapely.LineString(r['coordinates']),width=r['width_m'],properties=r['properties'])
                     for r in raw['roads']]
        roads,graphs,usage = plan(transport,recipes,widths)
        began = time.perf_counter()
        report = compile_travel(roads,graphs,recipes['travelRecipes'],motor_class=args.motor_class)
        elapsed_ms = (time.perf_counter()-began)*1000
        report.update(place=place,source_axes=len(transport),accepted_axes=len(roads),compile_ms=elapsed_ms,
                      source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),implementation_sha256=implementation)
        for note in report['source_notes']:
            note['id'] = roads[note['source']]['source_id']
        (args.output/f'{place}-travel.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps({k:report[k] for k in ('place','motor_vehicle_class','accepted_axes','compile_ms','public_directed_arcs',
                                               'source_note_reasons','unresolved_owner_directions')}))


if __name__=='__main__':
    main()
