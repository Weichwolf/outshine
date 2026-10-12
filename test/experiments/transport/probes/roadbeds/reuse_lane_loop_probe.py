"""Fit fixed source ports to an existing opposite-direction loop inside bounded public space."""

import argparse
import json
from pathlib import Path
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from hermite_station_count_probe import collision_fitter
from steering_roadbed_candidate_probe import candidate
from turning_space_probe import provenance
from transport_turn_roadbeds import roadbed_lateral_reach
from transport_spline_routes import evaluate_controls,curve_bounds,split_controls
from transport_cut_setback_experiment import digest,implementation_files


COLLISION_STATIONS=33
BEND_SAMPLES=8


def tighten_curvature_bound(controls,radius):
    for _ in range(3):
        bad=curve_bounds(controls)[2]*radius>1
        if not np.any(bad):break
        controls=np.concatenate([split_controls(c[None]) if split else c[None] for c,split in zip(controls,bad)])
    return controls


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--guide',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--reverse',action='store_true')
    parser.add_argument('--pedestrian-crossings',action='store_true')
    parser.add_argument('--spacing',type=float,nargs='+',default=[8.,4.])
    parser.add_argument('--evaluations',type=int,default=40)
    parser.add_argument('--curvature-weight',type=float,default=4.)
    args=parser.parse_args();data=json.loads(args.input.read_text());args.output.mkdir(parents=True,exist_ok=True)
    if not np.isfinite(args.curvature_weight) or args.curvature_weight<=0:parser.error('curvature weight must be finite and positive')
    if args.evaluations<1 or not all(np.isfinite(s) and s>0 for s in args.spacing):parser.error('fit budgets and spacings must be positive')
    guide=np.array(json.loads(args.guide.read_text())['path'])
    source=shapely.LineString(guide[::-1] if args.reverse else guide)
    floor,other=(shapely.from_wkb(data[k]) for k in ('surface','non_motor_reservations'))
    walk=shapely.from_wkb(data['public_pedestrian_surface']) if args.pedestrian_crossings else shapely.Polygon()
    hard=other.difference(walk).difference(floor);data['reservations']=hard.wkb.hex()
    domain=floor.buffer(roadbed_lateral_reach(data['vehicle'])-2*data['error_m']).difference(hard)
    entry,exit=({k:np.array(v) for k,v in data[name].items()} for name in ('entry','exit'))
    rows=[];started=time.perf_counter();fit=collision_fitter(COLLISION_STATIONS);captured=[]
    check=fit.__globals__['checked_spline_path']
    def record_check(controls,*values):
        controls=tighten_curvature_bound(controls,data['vehicle']['minimumRearAxleRadiusM'])
        captured.append(controls.copy())
        return check(controls,*values)
    fit.__globals__['checked_spline_path']=record_check
    solver=fit.__globals__['solve_hermite_system']
    def weighted_solver(residual,initial,**kwargs):
        segments=(len(initial)-2)//6+1
        def weighted(values):
            result=residual(values)
            width=5*COLLISION_STATIONS+BEND_SAMPLES;begin=4*COLLISION_STATIONS
            result[:segments*width].reshape(segments,width)[:,begin:begin+BEND_SAMPLES]*=args.curvature_weight/4
            return result
        return solver(weighted,initial,**kwargs)
    fit.__globals__['solve_hermite_system']=weighted_solver
    seen_counts=set()
    for spacing in args.spacing:
        count=max(3,min(18,int(np.ceil(source.length/spacing))+1))
        if count in seen_counts:continue
        seen_counts.add(count)
        captured.clear()
        path,note=fit(entry,exit,source,domain,data['vehicle'],data['error_m'],spacing=spacing,
            maximum_calls=1024,maximum_evaluations=args.evaluations,sample_body_edges=True,include_controls=True)
        row=dict(spacing_m=spacing,accepted=False,residual_calls=note['residual_calls'],reason=note.get('reason'),
                 optimizer_status=note['optimizer_status'],unweighted_objective=note['minimum_objective_seen'],curvature_weight=args.curvature_weight);rows.append(row)
        controls=captured[-1];owners=np.repeat(np.arange(len(controls)),129);stations=np.tile(np.linspace(0,1,129),len(controls))
        velocity=evaluate_controls(5*np.diff(controls[owners],axis=1),stations)
        acceleration=evaluate_controls(20*np.diff(controls[owners],n=2,axis=1),stations)
        curvature=np.abs(velocity[:,0]*acceleration[:,1]-velocity[:,1]*acceleration[:,0])/np.linalg.norm(velocity,axis=1)**3
        row.update(sampled_curvature_per_m=float(curvature.max()),curvature_bound_per_m=float(curve_bounds(controls)[2].max()))
        (args.output/f'{len(rows)-1}-controls.json').write_text(json.dumps(controls.tolist())+'\n')
        if path is None:continue
        note['curve_controls']=controls.tolist()
        try:road,_=candidate(data,note)
        except AssertionError as error:row['reason']=str(error);continue
        crossing=road['surface'].intersection(walk).difference(floor)
        row.update(accepted=True,length_m=road['path'].length,added_area_m2=road['addition'].area)
        (args.output/f'{len(rows)-1}-candidate.json').write_text(json.dumps(note,indent=2)+'\n')
        (args.output/f'{len(rows)-1}-geometry.json').write_text(json.dumps(dict(original=floor.wkb.hex(),
            planned=road['surface'].wkb.hex(),crossing=crossing.wkb.hex(),path=shapely.get_coordinates(road['path']).tolist()),indent=2)+'\n')
        print(json.dumps(row),flush=True)
    receipt=provenance()
    sources=[Path(__file__),Path(collision_fitter.__code__.co_filename),*implementation_files(Path(candidate.__code__.co_filename))]
    receipt['implementation_sha256'].update({str(p.resolve().relative_to(Path.cwd())):digest(p) for p in sources})
    report=dict(cases=rows,input_sha256={str(p):digest(p) for p in (args.input,args.guide)},
                elapsed_ms=(time.perf_counter()-started)*1000,**receipt)
    (args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(trials=len(rows),accepted=sum(r['accepted'] for r in rows))),flush=True)


if __name__=='__main__':main()
