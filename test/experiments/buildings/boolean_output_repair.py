"""Cancel opposite unresolvable facets with identical topology and source support."""

import numpy as np
from fractions import Fraction

from boolean_exact_regions import point


def zero_area(triangle):
    if np.any(np.cross(triangle[1]-triangle[0],triangle[2]-triangle[0])):return False
    a,b,c=map(point,triangle)
    u=tuple(y-x for x,y in zip(a,b,strict=True));v=tuple(y-x for x,y in zip(a,c,strict=True))
    return all(u[(i+1)%3]*v[(i+2)%3]==u[(i+2)%3]*v[(i+1)%3] for i in range(3))


def cycle(triangle):
    values=tuple(map(int,triangle))
    return min(values[i:]+values[:i] for i in range(3))


def unresolved_area(triangle,exact=None):
    """Bound cross-product error from arithmetic and known construction rounding.

    For rounded edges u/v and L1 error bounds du/dv, the additional bound is
    du*|v|1 + dv*|u|1 + du*dv. No coordinate-distance threshold is assumed.
    """
    u=triangle[1]-triangle[0];v=triangle[2]-triangle[0]
    uncertainty=8*np.finfo(float).eps*np.linalg.norm(abs(u[[1,2,0]]*v[[2,0,1]])+abs(u[[2,0,1]]*v[[1,2,0]]))
    a,b,c=map(point,triangle)
    first=tuple(y-x for x,y in zip(a,b,strict=True));last=tuple(y-x for x,y in zip(a,c,strict=True))
    normal=tuple(first[(i+1)%3]*last[(i+2)%3]-first[(i+2)%3]*last[(i+1)%3] for i in range(3))
    bound=Fraction(float(uncertainty))
    if exact is not None:
        errors=[sum(abs(value-original) for value,original in zip(p,q,strict=True)) for p,q in zip((a,b,c),exact,strict=True)]
        du,dv=errors[0]+errors[1],errors[0]+errors[2]
        bound+=du*sum(map(abs,last))+dv*sum(map(abs,first))+du*dv
    if sum(value*value for value in normal)>bound**2:return None
    longest=max(abs(value) for value in (*first,*last,*[y-x for x,y in zip(b,c,strict=True)]))
    if not longest:return 0.
    return float(np.nextafter(float(bound/longest),np.inf)) if bound else 0.


def cancel(roots,supports,native,nodes,*,evidence=None):
    keep=np.ones(len(roots),dtype=bool);pending={}
    for index,triangle in enumerate(roots):
        xyz=np.asarray([nodes.coordinates[int(root)] for root in triangle])
        exact=None if not hasattr(nodes,'exact') else tuple(nodes.exact[int(root)] for root in triangle)
        bound=unresolved_area(xyz,exact)
        if bound is None:continue
        key=(int(native[index]),supports[index],cycle(triangle))
        opposite=(key[0],key[1],cycle(triangle[[0,2,1]]))
        candidates=pending.get(opposite)
        if candidates:
            previous=candidates.pop();keep[previous]=keep[index]=False
            if evidence is not None:evidence['maximum_cancelled_altitude_m']=max(evidence.get('maximum_cancelled_altitude_m',0.),bound)
        else:pending.setdefault(key,[]).append(index)
    return roots[keep],tuple(s for s,k in zip(supports,keep,strict=True) if k),np.asarray(native,dtype=np.int32)[keep]
