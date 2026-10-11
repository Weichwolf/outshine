"""Compare obstacle-preserving fair source guides under the existing fitting budget."""

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
from infrastructure_route_fairing import fair_route
from infrastructure_source_roadbeds import construct_route_roadbed
from infrastructure_turn_roadbeds import roadbed_lateral_reach


def seed_choices(source,surface,reserved,vehicle):
    count=max(3,min(66,int(np.ceil(source.length/vehicle['minimumRearAxleRadiusM']))+1))
    points=shapely.get_coordinates(shapely.line_interpolate_point(source,np.linspace(0,source.length,count)))
    domain=surface.buffer(roadbed_lateral_reach(vehicle)).difference(reserved.difference(surface))
    choices=[]
    for weight in (.25,1.,4.,16.):
        path=shapely.LineString(fair_route(points,weight))
        if not path.is_simple or not same_island_winding(path,source,surface):continue
        outside=path.buffer(vehicle['widthM']*.5).difference(domain).area
        choices.append((outside,path.length,weight,path))
    return sorted(choices,key=lambda row:row[:2])


def compare(path,output):
    data=json.loads(path.read_text())
    entry,exit=({k:np.array(v) for k,v in data[p].items()} for p in ('entry','exit'))
    surface,reserved,source=(shapely.from_wkb(data[k]) for k in ('surface','reservations','source_route'))
    vehicle,error=data['vehicle'],data['error_m']
    begin=time.perf_counter()
    baseline,trials=construct_route_roadbed(entry,exit,surface,reserved,source,vehicle,error)
    baseline_ms=(time.perf_counter()-begin)*1000
    choices=seed_choices(source,surface,reserved,vehicle)
    rows=[];candidate=None
    begin=time.perf_counter()
    if choices:
        outside,length,weight,seed=choices[0]
        candidate,note=construct_fitted_roadbed(entry,exit,surface,reserved,seed,vehicle,error,maximum_calls=512)
        if candidate is not None and not same_island_winding(candidate['path'],source,surface):
            candidate=None;note=dict(note,reason='fair_seed_changes_source_winding')
        rows.append(dict(weight=weight,seed_length_m=length,seed_outside_area_m2=outside,
                         residual_calls=note['residual_calls'],reason=note.get('reason')))
    row=dict(input=str(path),input_sha256=digest(path),baseline_valid=baseline is not None,
        baseline_elapsed_ms=baseline_ms,baseline_residual_calls=sum(t.get('residual_calls',0) for t in trials),
        fair_valid=candidate is not None,fair_elapsed_ms=(time.perf_counter()-begin)*1000,trials=rows,
        ranked_choices=[dict(outside_m2=a,length_m=b,weight=c) for a,b,c,_ in choices])
    if candidate is not None:
        output.mkdir(parents=True,exist_ok=True)
        def encode(value):
            if isinstance(value,shapely.Geometry):return value.wkb.hex()
            if isinstance(value,np.ndarray):return value.tolist()
            if isinstance(value,np.generic):return value.item()
            raise TypeError(type(value).__name__)
        (output/'candidate.json').write_text(json.dumps(candidate,indent=2,default=encode)+'\n')
        row['candidate']=str(output/'candidate.json')
    return row


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,nargs='+',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);rows=[]
    for path in args.input:
        row=compare(path,args.output/path.stem);rows.append(row);print(json.dumps(row),flush=True)
    sources=implementation_files(Path(__file__))
    result=dict(cases=rows,measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},
        scope='512-call replacement for tangent-circle starts after the 1024-call source fitting stage; whole coupled preservation remains required.')
    (args.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':main()
