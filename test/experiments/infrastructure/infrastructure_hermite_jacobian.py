"""Use the minimum column grouping for neighboring six-field Hermite nodes."""

import numpy as np
from scipy.optimize import least_squares
from scipy.sparse import csr_matrix


def hermite_column_groups(variables):
    if variables<8 or (variables-2)%6:
        raise ValueError('expected six fields per internal node and two endpoint scales')
    count=(variables-2)//6+2
    nodes=np.arange(1,count-1)
    return np.r_[(6*(nodes%2)[:,None]+np.arange(6)).ravel(),0,6*((count-1)%2)]


def solve_hermite_system(fun,initial,*,jac_sparsity,bounds,**kwargs):
    pattern=jac_sparsity.tocsr()
    groups=hermite_column_groups(len(initial))
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
            steps=np.zeros(len(values));steps[selected]=trial[selected]-values[selected]
            data[part]=difference[rows[part]]/steps[columns[part]]
        return csr_matrix((data,columns,pattern.indptr),shape=pattern.shape)
    return least_squares(measured,initial,jac=jacobian,bounds=bounds,**kwargs)
