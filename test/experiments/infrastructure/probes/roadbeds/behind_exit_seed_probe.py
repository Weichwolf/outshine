"""Compare bounded tangent-circle seeds for routes whose exit lies behind their entry."""

import argparse
from itertools import chain
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from infrastructure_arc_line_paths import arc_line_arc_seeds
from infrastructure_cut_setback_experiment import digest,implementation_files
from infrastructure_fitted_roadbeds import construct_fitted_roadbed
from infrastructure_junction_surfaces import same_island_winding
from infrastructure_source_roadbeds import construct_route_roadbed
from infrastructure_three_arc_paths import three_arc_seeds
from infrastructure_turn_roadbeds import roadbed_lateral_reach


def behind_exit_roadbed(entry,exit,surface,reserved,source,vehicle,error,maximum_calls=512):
    if maximum_calls<1:
        return None,dict(reason='fitting_budget_exhausted',residual_calls=0)
    if np.dot(exit['position']-entry['position'],entry['heading'])>=0:
        return None,dict(reason='exit_not_behind_entry',residual_calls=0)
    domain=surface.buffer(roadbed_lateral_reach(vehicle)).difference(reserved.difference(surface))
    options=[]
    for scale in (1.05,1.125,1.25):
        radius=scale*vehicle['minimumRearAxleRadiusM']
        families=chain(((('CCC',*kind),seed) for kind,seed in three_arc_seeds(entry,exit,radius)),
                       ((('CSC',*kind),seed) for kind,seed in arc_line_arc_seeds(entry,exit,radius)))
        for kind,seed in families:
            if not same_island_winding(seed,source,surface):continue
            outside=seed.buffer(vehicle['widthM']*.5).difference(domain).area
            options.append((outside,seed.length,radius,kind,seed))
    trials=[];calls=0
    for outside,length,radius,kind,seed in sorted(options,key=lambda row:row[:2])[:1]:
        candidate,note=construct_fitted_roadbed(entry,exit,surface,reserved,seed,vehicle,error,maximum_calls=maximum_calls)
        calls+=note['residual_calls']
        if candidate is not None and not same_island_winding(candidate['path'],source,surface):
            candidate=None;note=dict(note,accepted=False,reason='seed_changes_source_winding')
        trials.append(dict(seed_kind=kind,seed_radius_m=radius,seed_length_m=length,seed_outside_area_m2=outside,
                           reason=note.get('reason'),residual_calls=note['residual_calls']))
        if candidate is not None:
            return candidate,dict(reason='valid_behind_exit_seed',residual_calls=calls,trials=trials)
    return None,dict(reason='no_valid_behind_exit_seed',residual_calls=calls,trials=trials)


def compare(path,output):
    data=json.loads(path.read_text())
    entry,exit=({k:np.array(v) for k,v in data[p].items()} for p in ('entry','exit'))
    surface,reserved,source=(shapely.from_wkb(data[k]) for k in ('surface','reservations','source_route'))
    vehicle,error=data['vehicle'],data['error_m']
    began=time.perf_counter()
    original,trials=construct_route_roadbed(entry,exit,surface,reserved,source,vehicle,error)
    original_ms=(time.perf_counter()-began)*1000
    calls=sum(t.get('residual_calls',0) for t in trials)
    remaining=min(512,1536-calls)
    began=time.perf_counter()
    candidate,note=behind_exit_roadbed(entry,exit,surface,reserved,source,vehicle,error,remaining)
    assert calls+note['residual_calls']<=1536
    row=dict(input=str(path),input_sha256=digest(path),heading_dot=float(np.dot(entry['heading'],exit['heading'])),
             forward_distance_m=float(np.dot(exit['position']-entry['position'],entry['heading'])),
             original_valid=original is not None,original_elapsed_ms=original_ms,original_residual_calls=calls,
             seeded_valid=candidate is not None,seeded_elapsed_ms=(time.perf_counter()-began)*1000,**note)
    if candidate is not None:
        output.mkdir(parents=True,exist_ok=True)
        result=dict(pieces=candidate['pieces'],path=shapely.get_coordinates(candidate['path']).tolist(),
                    public_surface_wkb=candidate['surface'].wkb.hex(),report=candidate['report'])
        (output/'candidate.json').write_text(json.dumps(result,indent=2,default=lambda obj:obj.tolist())+'\n')
        row['candidate']=str(output/'candidate.json')
    return row


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,nargs='+',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    rows=[]
    for path in args.input:
        row=compare(path,args.output/f'{path.parent.name}-{path.stem}')
        rows.append(row);print(json.dumps(row),flush=True)
    sources=set(implementation_files(Path(__file__)))|set(implementation_files(
                 Path(__file__).resolve().parents[2]/'infrastructure_source_roadbeds.py'))
    result=dict(measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},cases=rows)
    (args.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':main()
