"""Compare steering guides against real reservations and final conservative curve checks."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from infrastructure_cut_setback_experiment import digest,implementation_files
from infrastructure_fitted_roadbeds import construct_fitted_roadbed
from infrastructure_junction_surfaces import same_island_winding
from infrastructure_steering_guide import steering_guide,steering_points,crossing_signature
from infrastructure_turn_roadbeds import roadbed_lateral_reach


def verify():
    points,angles=steering_points(np.array([2.,3.,0.]),0,4.,5.)
    assert np.array_equal(points,[[2,3],[4,3],[6,3]]) and np.all(angles==0)
    for sign in (-1,1):
        points,angles=steering_points(np.array([0.,0.,0.]),sign,5*np.pi/2,5.)
        assert np.allclose(points[-1],[5,sign*5]) and abs(angles[-1]-sign*np.pi/2)<1e-12
    loop=np.array([[-1,-1],[1,-1],[1,1],[-1,1],[-1,-1]])
    markers=np.array([[0.,0.],[2.,0.]])
    assert crossing_signature(loop,markers)==(1,0) and crossing_signature(loop[::-1],markers)==(-1,0)
    return dict(exact_straight_and_circle=True,oriented_ray_crossings=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,nargs='+',required=True)
    parser.add_argument('--maximum-states',type=int,default=20000)
    parser.add_argument('--cell',type=float)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);rows=[]
    checks=verify()
    for path in args.input:
        data=json.loads(path.read_text())
        entry,exit=({k:np.array(v) for k,v in data[p].items()} for p in ('entry','exit'))
        surface,reserved,source=(shapely.from_wkb(data[k]) for k in ('surface','reservations','source_route'))
        vehicle,error=data['vehicle'],data['error_m']
        domain=surface.buffer(roadbed_lateral_reach(vehicle)-2*error).difference(reserved.difference(surface).buffer(2*error))
        begin=time.perf_counter()
        guide,note=steering_guide(entry,exit,source,surface,domain,vehicle,maximum_states=args.maximum_states,cell=args.cell)
        row=dict(input=str(path),input_sha256=digest(path),guide_found=guide is not None,
                 search_elapsed_ms=(time.perf_counter()-begin)*1000,search=note,accepted=False)
        if guide is not None:
            assert same_island_winding(guide,source,surface)
            begin=time.perf_counter()
            candidate,fit=construct_fitted_roadbed(entry,exit,surface,reserved,guide,vehicle,error,maximum_calls=1024)
            row.update(accepted=candidate is not None,fit_elapsed_ms=(time.perf_counter()-begin)*1000,
                       residual_calls=fit['residual_calls'],fit_reason=fit.get('reason'),guide_length_m=guide.length)
            if candidate is not None:
                assert same_island_winding(candidate['path'],source,surface)
                output=args.output/path.stem;output.mkdir(exist_ok=True)
                def encode(value):
                    if isinstance(value,shapely.Geometry):return value.wkb.hex()
                    if isinstance(value,np.ndarray):return value.tolist()
                    if isinstance(value,np.generic):return value.item()
                    raise TypeError(type(value).__name__)
                (output/'candidate.json').write_text(json.dumps(candidate,indent=2,default=encode)+'\n')
            (args.output/f'{path.stem}-guide.json').write_text(json.dumps(dict(path=shapely.get_coordinates(guide).tolist()))+'\n')
        rows.append(row);print(json.dumps(row),flush=True)
    sources=implementation_files(Path(__file__))
    report=dict(checks=checks,cases=rows,measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},
                scope='Bounded sampled steering search and conservative G2/body refinement; native coupled preservation remains required.')
    (args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()
