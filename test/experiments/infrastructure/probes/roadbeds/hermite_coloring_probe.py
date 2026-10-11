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
from scipy.sparse import csr_matrix

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_hermite_route_fitting as fitting
from infrastructure_cut_setback_experiment import implementation_files,digest


def chain_colors(variables):
    if variables<8 or (variables-2)%6:
        raise ValueError('expected six fields per internal node and two endpoint scales')
    count=(variables-2)//6+2
    nodes=np.arange(1,count-1)
    groups=np.r_[(6*(nodes%2)[:,None]+np.arange(6)).ravel(),0,6*((count-1)%2)]
    return groups


def colored_least_squares(fun,initial,*,jac_sparsity,bounds,**kwargs):
    pattern=jac_sparsity.tocsr()
    groups=chain_colors(len(initial))
    rows=np.repeat(np.arange(pattern.shape[0]),np.diff(pattern.indptr))
    columns=pattern.indices
    groups_present=np.unique(groups)
    parts=[np.flatnonzero(groups[columns]==group) for group in groups_present]
    cache={}
    def measured(values):
        result=fun(values)
        cache.update(values=values.copy(),residual=result)
        return result
    def jacobian(values):
        if not np.array_equal(cache.get('values'),values):measured(values)
        base=cache['residual'].copy()
        step=np.sqrt(np.finfo(values.dtype).eps)*np.where(values>=0,1.,-1.)*np.maximum(1.,np.abs(values))
        if np.any(values+step<bounds[0]) or np.any(values+step>bounds[1]):
            raise ValueError('difference step leaves supported bounds')
        data=np.empty(pattern.nnz)
        for group,part in zip(groups_present,parts):
            selected=groups==group
            trial=values.copy();trial[selected]+=step[selected]
            difference=measured(trial)-base
            actual_step=trial[selected]-values[selected]
            steps=np.zeros(len(values));steps[selected]=actual_step
            data[part]=difference[rows[part]]/steps[columns[part]]
        return csr_matrix((data,columns,pattern.indptr),shape=pattern.shape)
    return least_squares(measured,initial,jac=jacobian,bounds=bounds,**kwargs)


def verify_coloring():
    cases=[]
    rng=np.random.default_rng(2343)
    for count in (3,4,6,12,18):
        pattern=fitting.residual_dependencies(count)
        colors=chain_colors(pattern.shape[1])
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
        original=colored_least_squares.__globals__['least_squares']
        def capture(fun,initial,*,jac,**kwargs):
            fun(initial);captured['jacobian']=jac(initial)
        colored_least_squares.__globals__['least_squares']=capture
        try:colored_least_squares(objective,state,jac_sparsity=pattern,bounds=(lower,np.inf))
        finally:colored_least_squares.__globals__['least_squares']=original
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
        for name,method in (('baseline',least_squares),('chain',colored_least_squares)):
            fitting.least_squares=method
            begin=time.perf_counter()
            try:curve,note=fitting.fitted_route_candidates(*poses,route,surface,data['vehicle'],data['error_m'],
                                                          args.budget,include_controls=True)
            finally:fitting.least_squares=least_squares
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
