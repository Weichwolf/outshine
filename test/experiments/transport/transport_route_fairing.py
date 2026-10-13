"""Minimize source displacement and discrete bending with fixed endpoints in O(N)."""

import numpy as np
from scipy.linalg import solveh_banded


def fair_route(points,weight):
    if weight<0 or not np.isfinite(weight):
        raise ValueError('fairing weight must be finite and nonnegative')
    n = len(points)
    if n<3 or weight==0:
        return points.copy()
    diagonal = np.ones(n)
    diagonal[:-2] += weight
    diagonal[1:-1] += 4*weight
    diagonal[2:] += weight
    adjacent = np.zeros(n-1)
    adjacent[:-1] -= 2*weight
    adjacent[1:] -= 2*weight
    m = n-2
    bands = np.zeros((3,m))
    bands[0] = diagonal[1:-1]
    bands[1,:m-1] = adjacent[1:-1]
    if m>2:
        bands[2,:m-2] = weight
    local = points-points[0]
    rhs = local[1:-1].copy()
    rhs[-1] -= adjacent[-1]*local[-1]
    if m>1:
        rhs[-2] -= weight*local[-1]
    result = points.copy()
    result[1:-1] = solveh_banded(bands,rhs,lower=True)+points[0]
    return result
