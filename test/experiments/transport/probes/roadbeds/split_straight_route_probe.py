"""Fit short terminal transitions around one straight interior with a shared call budget."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely
from shapely.ops import substring

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from coupled_turning_court_probe import approach_segment
from steering_roadbed_candidate_probe import candidate
from transport_cut_setback_experiment import digest,implementation_files
from transport_hermite_route_fitting import fit_hermite_route
from transport_spline_routes import continuous_spline_joins
from transport_turn_roadbeds import roadbed_lateral_reach


def split_controls(entry,exit,source,domain,vehicle,error,head,tail,maximum_calls):
    if head<=0 or tail<=0 or head+tail>=source.length:
        return None,dict(reason='invalid_terminal_lengths',residual_calls=0)
    endpoints=np.array([source.interpolate(head).coords[0],source.interpolate(source.length-tail).coords[0]])
    direction=endpoints[1]-endpoints[0]
    length=np.linalg.norm(direction)
    if length<=error:return None,dict(reason='collapsed_straight_interior',residual_calls=0)
    direction/=length
    inner=[dict(position=p,heading=direction) for p in endpoints]
    pieces=[];calls=0;notes=[]
    local_routes=[substring(source,0,head),substring(source,source.length-tail,source.length)]
    for index,(begin,end,route) in enumerate(zip((entry,inner[1]),(inner[0],exit),local_routes)):
        budget=(maximum_calls-calls)//(2-index)
        if budget<1:return None,dict(reason='terminal_call_budget_exhausted',residual_calls=calls)
        path,note=fit_hermite_route(begin,end,route,domain,vehicle,error,maximum_calls=budget,include_controls=True)
        calls+=note['residual_calls'];notes.append({k:v for k,v in note.items() if k!='curve_controls'})
        if path is None:return None,dict(reason=note['reason'],residual_calls=calls,terminal_fits=notes)
        pieces.append(np.array(note['curve_controls']))
    controls=np.concatenate([pieces[0],approach_segment(*inner)[None],pieces[1]])
    assert continuous_spline_joins(controls),'split terminal tangents or curvature differ'
    return controls,dict(reason='terminal_curves_fitted',residual_calls=calls,terminal_fits=notes,
                         straight_length_m=float(length),control_segments=len(controls))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--guide',type=Path)
    parser.add_argument('--head',type=float,nargs='+',default=[8.,16.,32.])
    parser.add_argument('--tail',type=float,nargs='+',default=[8.,16.])
    parser.add_argument('--maximum-calls',type=int,default=1024)
    parser.add_argument('--compare-uniform',action='store_true')
    parser.add_argument('--pedestrian-crossings',action='store_true')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.maximum_calls<2 or len(args.head)*len(args.tail)>12:
        parser.error('positive call budget and at most twelve split cases required')
    if not all(np.isfinite(x) and x>0 for x in [*args.head,*args.tail]):
        parser.error('terminal lengths must be finite and positive')
    data=json.loads(args.input.read_text());args.output.mkdir(parents=True,exist_ok=True)
    source=shapely.from_wkb(data['source_route'])
    guide=shapely.LineString(json.loads(args.guide.read_text())['path']) if args.guide else source
    entry,exit=({k:np.array(v) for k,v in data[name].items()} for name in ('entry','exit'))
    floor=shapely.from_wkb(data['surface']);reserved=shapely.from_wkb(data['reservations']);walk=shapely.Polygon()
    if args.pedestrian_crossings:
        walk=shapely.from_wkb(data['public_pedestrian_surface'])
        reserved=shapely.from_wkb(data['non_motor_reservations']).difference(walk).difference(floor)
        data['reservations']=reserved.wkb.hex()
    error=data['error_m'];vehicle=data['vehicle'];rows=[]
    expanded=floor.buffer(roadbed_lateral_reach(vehicle)-2*error)
    domain=floor.union(expanded.difference(reserved.difference(floor).buffer(2*error)))
    uniform=None
    if args.compare_uniform:
        started=time.perf_counter()
        path,note=fit_hermite_route(entry,exit,guide,domain,vehicle,error,maximum_calls=args.maximum_calls,include_controls=True)
        uniform=dict(accepted=False,reason=note['reason'] if path is None else 'fitted',residual_calls=note['residual_calls'])
        if path is not None:
            try:road,_=candidate(data,note)
            except AssertionError as reason:uniform['reason']=str(reason)
            else:uniform.update(accepted=True,reason='proved',length_m=road['path'].length,added_area_m2=road['addition'].area,
                                control_segments=len(note['curve_controls']),hull_intervals=road['construction']['hull_intervals'])
        uniform['elapsed_ms']=(time.perf_counter()-started)*1000
        print(json.dumps(dict(method='uniform',**uniform)),flush=True)
    for head in args.head:
        for tail in args.tail:
            begin=time.perf_counter()
            controls,note=split_controls(entry,exit,guide,domain,vehicle,error,head,tail,args.maximum_calls)
            row=dict(head_m=head,tail_m=tail,accepted=False,**note);index=len(rows);rows.append(row)
            if controls is not None:
                fit=dict(curve_controls=controls.tolist(),road_space_recipe='lane-flare',**note)
                try:road,_=candidate(data,fit)
                except AssertionError as reason:row['reason']=str(reason)
                else:
                    row.update(accepted=True,reason='proved',length_m=road['path'].length,added_area_m2=road['addition'].area,
                               hull_intervals=road['construction']['hull_intervals'])
                    (args.output/f'{index}-candidate.json').write_text(json.dumps(fit,indent=2)+'\n')
                    (args.output/f'{index}-geometry.json').write_text(json.dumps(dict(original=floor.wkb.hex(),
                        planned=road['surface'].wkb.hex(),crossing=road['roadbed'].intersection(walk).difference(floor).wkb.hex(),
                        path=shapely.get_coordinates(road['path']).tolist()))+'\n')
            row['elapsed_ms']=(time.perf_counter()-begin)*1000
            print(json.dumps({k:v for k,v in row.items() if k!='terminal_fits'}),flush=True)
    files=implementation_files(Path(__file__))
    report=dict(cases=rows,uniform=uniform,input=str(args.input),input_sha256=digest(args.input),maximum_calls_per_case=args.maximum_calls,
                guide=str(args.guide) if args.guide else None,guide_sha256=digest(args.guide) if args.guide else None,
                measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in files},
                scope='Local terminal fits plus straight interior; full curve/body/site checks; joint network preservation still required.')
    (args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()
