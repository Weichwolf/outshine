"""Compare optimal chain coloring with the general sparse finite-difference grouping."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely
from scipy.optimize import least_squares

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_hermite_route_fitting as fitting
import infrastructure_hermite_jacobian as jacobian
from infrastructure_cut_setback_experiment import implementation_files,digest


def verify_coloring():
    cases=[]
    rng=np.random.default_rng(2343)
    for count in (3,4,6,12,18):
        pattern=fitting.residual_dependencies(count)
        colors=jacobian.hermite_column_groups(pattern.shape[1])
        rows=np.repeat(np.arange(pattern.shape[0]),np.diff(pattern.indptr))
        for group in np.unique(colors):
            selected=rows[colors[pattern.indices]==group]
            assert len(np.unique(selected))==len(selected),'dependent fields share a color'
        lower_bound=int(np.diff(pattern.indptr).max())
        assert len(np.unique(colors))==lower_bound
        matrix=pattern.copy().astype(float)
        matrix.data=rng.normal(size=pattern.nnz)
        state=rng.normal(size=pattern.shape[1]);state[-2:]=1.
        target=matrix@state
        lower=np.full(len(state),-np.inf);lower[-2:]=.1
        captured={}
        def objective(values):return matrix@values-target
        original=jacobian.least_squares
        def capture(fun,initial,*,jac,**kwargs):
            fun(initial);captured['jacobian']=jac(initial)
        jacobian.least_squares=capture
        try:jacobian.solve_hermite_system(objective,state,jac_sparsity=pattern,bounds=(lower,np.inf))
        finally:jacobian.least_squares=original
        assert np.allclose(captured['jacobian'].toarray(),matrix.toarray(),atol=2e-7,rtol=2e-7)
        cases.append(dict(nodes=count,groups=len(np.unique(colors)),row_clique_lower_bound=lower_bound))
    return dict(cases=cases,independent_groups=True,minimum_group_count_proved=True,linear_jacobian_checked=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,nargs='+',required=True)
    parser.add_argument('--budget',type=int,default=1024)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();checks=verify_coloring();rows=[]
    for path in args.input:
        data=json.loads(path.read_text())
        poses=[{k:np.array(data[name][k]) for k in ('position','heading')} for name in ('entry','exit')]
        route,surface=(shapely.from_wkb(data[name]) for name in ('source_route','surface'))
        original=fitting.solve_hermite_system
        for name,method in (('baseline',least_squares),('chain',jacobian.solve_hermite_system)):
            fitting.solve_hermite_system=method
            begin=time.perf_counter()
            try:curve,note=fitting.fitted_route_candidates(*poses,route,surface,data['vehicle'],data['error_m'],
                                                          args.budget,include_controls=True)
            finally:fitting.solve_hermite_system=original
            assert note['residual_calls']<=args.budget
            note.pop('curve_controls',None)
            row=dict(input=str(path),method=name,accepted=curve is not None,
                     path_sha256=hashlib.sha256(curve.wkb).hexdigest() if curve is not None else None,
                     elapsed_ms=(time.perf_counter()-begin)*1000,residual_calls=note['residual_calls'],report=note)
            rows.append(row)
            print(json.dumps({k:row[k] for k in ('input','method','accepted','elapsed_ms','residual_calls','path_sha256')}),flush=True)
    args.output.mkdir(parents=True,exist_ok=True)
    sources=implementation_files(Path(__file__))+implementation_files(Path(fitting.__file__))
    report=dict(checks=checks,rows=rows,input_sha256={str(p):digest(p) for p in args.input},
                measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                implementation_sha256={str(p.relative_to(Path.cwd())):digest(p) for p in sources},
                scope='Frozen local routes and actual vehicle checks; whole-network preservation remains required.')
    (args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()
