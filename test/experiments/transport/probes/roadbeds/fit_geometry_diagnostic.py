"""Locate actual curvature and body violations in rejected Hermite fits."""

import argparse
import inspect
import json
from pathlib import Path
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_hermite_route_fitting as fitting
from transport_spline_routes import curve_bounds,evaluate_controls,split_controls
from transport_vehicle_sweep import body_outlines


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,nargs='+',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);rows=[];saved=[]
    source=inspect.getsource(fitting.fit_hermite_route)
    anchor='    controls = decode(values)\n    for _ in range(3):'
    assert source.count(anchor)==1
    source=source.replace(anchor,'    controls = decode(values)\n    record_fit(controls.copy(),status)\n    for _ in range(3):')
    namespace=dict(fitting.__dict__,record_fit=lambda controls,status:saved.append((controls,status)))
    exec(compile(source,str(Path(__file__)), 'exec'),namespace)
    original=fitting.fit_hermite_route;fitting.fit_hermite_route=namespace['fit_hermite_route']
    try:
        for path in args.input:
            data=json.loads(path.read_text());saved.clear()
            poses=[{k:np.array(v) for k,v in data[p].items()} for p in ('entry','exit')]
            surface,route=(shapely.from_wkb(data[k]) for k in ('surface','source_route'))
            _,note=fitting.fitted_route_candidates(*poses,route,surface,data['vehicle'],data['error_m'],1024)
            for index,(controls,status) in enumerate(saved):
                owners=np.repeat(np.arange(len(controls)),129);stations=np.tile(np.linspace(0,1,129),len(controls))
                points=evaluate_controls(controls[owners],stations)
                velocity=evaluate_controls(5*np.diff(controls[owners],axis=1),stations)
                acceleration=evaluate_controls(20*np.diff(controls[owners],n=2,axis=1),stations)
                speed=np.linalg.norm(velocity,axis=1)
                curvature=np.abs(velocity[:,0]*acceleration[:,1]-velocity[:,1]*acceleration[:,0])/speed**3
                outlines=body_outlines(points,np.arctan2(velocity[:,1],velocity[:,0]),data['vehicle']['widthM']*.5,
                                      data['vehicle']['frontFromRearAxleM'],data['vehicle']['rearOverhangM'])
                outside=shapely.area(shapely.difference(shapely.polygons(outlines),surface))
                refined=controls
                for _ in range(3):refined=split_controls(refined)
                bound=float(curve_bounds(refined)[2].max());peak=int(curvature.argmax())
                row=dict(input=str(path),trial=index,status=status,maximum_sampled_curvature_per_m=float(curvature.max()),
                         curvature_bound_per_m=bound if np.isfinite(bound) else None,
                         peak_curvature_position=points[peak].tolist(),maximum_body_outside_m2=float(outside.max()),
                         peak_outside_position=points[outside.argmax()].tolist(),minimum_speed=float(speed.min()),
                         rejection=note['fit_trials'][index].get('reason'))
                rows.append(row);print(json.dumps(row),flush=True)
                (args.output/f'{path.stem}-{index}-controls.json').write_text(json.dumps(controls.tolist())+'\n')
    finally:fitting.fit_hermite_route=original
    (args.output/'results.json').write_text(json.dumps(rows,indent=2)+'\n')


if __name__=='__main__':main()
