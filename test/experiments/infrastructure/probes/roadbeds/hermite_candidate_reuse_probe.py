"""Compare bounded route fitting with completed identical seeds reused locally."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_hermite_route_fitting as fitting
from infrastructure_surface_clearance import SurfaceClearance
from infrastructure_cut_setback_experiment import implementation_files,digest


def reuse_candidates(entry,exit,route,surface,vehicle,error=.025,maximum_calls=1024,*,clearance=None,
                     include_controls=False):
    clearance=SurfaceClearance(surface) if clearance is None else clearance
    initial_samples,initial_measured=clearance.samples,clearance.measured_samples
    calls,legacy_calls=0,0
    trials,completed,skipped,unfinished=[],{},[],[]
    def result(path=None,note=None,spacing=None):
        summary=dict(residual_calls=calls,maximum_residual_calls=maximum_calls,fit_trials=trials,
                     clearance_samples=clearance.samples-initial_samples,
                     clearance_measured_samples=clearance.measured_samples-initial_measured,
                     identical_candidates_skipped=skipped,legacy_residual_calls=legacy_calls)
        if path is not None:return path,dict(note,spacing_m=spacing,**summary)
        return None,dict(reason='no_clear_fitted_route',**summary)
    candidates=[(spacing,False) for spacing in (vehicle['minimumRearAxleRadiusM']*2,vehicle['minimumRearAxleRadiusM'])]
    for spacing,sample_body_edges in candidates:
        remaining=maximum_calls-legacy_calls
        if remaining<=0:break
        nodes=max(3,min(18,int(np.ceil(route.length/spacing))+1))
        key=(nodes,sample_body_edges)
        previous=completed.get(key)
        if previous is not None and remaining>=previous['residual_calls']:
            skipped.append(dict(spacing_m=spacing,nodes=nodes,body_edges=sample_body_edges,
                                avoided_residual_calls=previous['residual_calls']))
            path=None
            note=dict(previous,residual_calls=0,maximum_residual_calls=remaining,
                      clearance_samples=0,clearance_measured_samples=0)
            legacy_calls+=previous['residual_calls']
        else:
            path,note=fitting.fit_hermite_route(entry,exit,route,surface,vehicle,error,spacing=spacing,
                                maximum_calls=remaining,clearance=clearance,sample_body_edges=sample_body_edges,
                                include_controls=include_controls)
            legacy_calls+=note['residual_calls']
            if note.get('optimizer_status') in ('optimizer_finished','optimizer_converged','optimizer_evaluation_budget_exhausted'):
                completed[key]=note
            elif note.get('optimizer_status')=='optimizer_residual_budget_exhausted':
                unfinished.append((spacing,sample_body_edges,note['residual_calls']))
        calls+=note['residual_calls']
        trials.append(dict(spacing_m=spacing,**{k:v for k,v in note.items() if k!='curve_controls'}))
        if not sample_body_edges and path is None and note.get('reason')=='spline_vehicle_body_outside_surface':
            candidates.append((spacing,True))
        if path is not None:return result(path,note,spacing)
    for spacing,sample_body_edges,previous_calls in unfinished:
        remaining=maximum_calls-calls
        if remaining<=previous_calls:continue
        path,note=fitting.fit_hermite_route(entry,exit,route,surface,vehicle,error,spacing=spacing,
                            maximum_calls=remaining,clearance=clearance,sample_body_edges=sample_body_edges,
                            include_controls=include_controls)
        calls+=note['residual_calls']
        trials.append(dict(spacing_m=spacing,saved_budget_retry=True,
                           **{k:v for k,v in note.items() if k!='curve_controls'}))
        if path is not None:return result(path,note,spacing)
    return result()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--budgets',type=int,nargs='+',default=[512,600,1024,1536])
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    data=json.loads(args.input.read_text())
    poses=[{k:np.array(data[name][k]) for k in ('position','heading')} for name in ('entry','exit')]
    route=shapely.from_wkb(data['source_route']);surface=shapely.from_wkb(data['surface'])
    rows=[]
    for budget in args.budgets:
        baseline=None;baseline_note=None
        for name,method in (('baseline',fitting.fitted_route_candidates),('reuse',reuse_candidates)):
            began=time.perf_counter()
            path,note=method(*poses,route,surface,data['vehicle'],data['error_m'],budget,include_controls=True)
            controls=note.pop('curve_controls',None)
            fingerprint=hashlib.sha256(path.wkb).hexdigest() if path is not None else None
            assert note['residual_calls']<=budget
            assert sum(t['residual_calls'] for t in note['fit_trials'])==note['residual_calls']
            if name=='baseline':baseline,baseline_note=fingerprint,note
            else:
                if baseline is not None:assert fingerprint==baseline,'previous valid fit changed'
                if not note['identical_candidates_skipped']:
                    assert {k:v for k,v in note.items() if k not in ('identical_candidates_skipped','legacy_residual_calls')}==baseline_note
            row=dict(method=name,budget=budget,accepted=path is not None,path_sha256=fingerprint,
                     elapsed_ms=(time.perf_counter()-began)*1000,report=note)
            if path is not None:row.update(curve_controls=controls,path_wkb=path.wkb.hex())
            rows.append(row)
            print(json.dumps({k:row[k] for k in ('method','budget','accepted','elapsed_ms')}|
                dict(residual_calls=note['residual_calls'],skips=len(note.get('identical_candidates_skipped',[])))),flush=True)
    sources=implementation_files(Path(__file__))
    report=dict(rows=rows,input=str(args.input),input_sha256=digest(args.input),
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},
        scope='One frozen published route; full network regression remains required.')
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()
