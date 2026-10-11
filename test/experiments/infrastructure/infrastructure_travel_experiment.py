#!/usr/bin/env python3
"""Check transport permissions on small counterexamples and cached real windows."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import shortest_path

from infrastructure_flat_plan import plan
from infrastructure_network import transport_widths
from infrastructure_travel import MODES,compile_travel,source_travel


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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('places',nargs='*')
    args = parser.parse_args()
    if args.places and args.input is None:
        parser.error('places require --input')
    args.output.mkdir(parents=True,exist_ok=True)
    configuration = Path(__file__).with_name('infrastructure_network_recipes.json')
    recipes = json.loads(configuration.read_text())
    implementation = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
                      (Path(__file__),Path(__file__).with_name('infrastructure_travel.py'),configuration)}
    (args.output/'small.json').write_text(json.dumps(verify(recipes),indent=2)+'\n')
    widths = transport_widths()
    for place in args.places:
        path = args.input/f'{place}-input.json'
        raw = json.loads(path.read_text())
        transport = [dict(line=shapely.LineString(r['coordinates']),width=r['width_m'],properties=r['properties'])
                     for r in raw['roads']]
        roads,graphs,usage = plan(transport,recipes,widths)
        began = time.perf_counter()
        report = compile_travel(roads,graphs,recipes['travelRecipes'])
        elapsed_ms = (time.perf_counter()-began)*1000
        report.update(place=place,source_axes=len(transport),accepted_axes=len(roads),compile_ms=elapsed_ms,
                      source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),implementation_sha256=implementation)
        for note in report['source_notes']:
            note['id'] = roads[note['source']]['source_id']
        (args.output/f'{place}-travel.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps({k:report[k] for k in ('place','accepted_axes','compile_ms','public_directed_arcs',
                                               'source_note_reasons','unresolved_owner_directions')}))


if __name__=='__main__':
    main()
