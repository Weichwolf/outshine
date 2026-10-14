"""Cancel opposite zero-area facets with identical topology and source support."""

import numpy as np

from boolean_exact_regions import point


def zero_area(triangle):
    if np.any(np.cross(triangle[1]-triangle[0],triangle[2]-triangle[0])):return False
    a,b,c=map(point,triangle)
    u=tuple(y-x for x,y in zip(a,b,strict=True));v=tuple(y-x for x,y in zip(a,c,strict=True))
    return all(u[(i+1)%3]*v[(i+2)%3]==u[(i+2)%3]*v[(i+1)%3] for i in range(3))


def cycle(triangle):
    values=tuple(map(int,triangle))
    return min(values[i:]+values[:i] for i in range(3))


def cancel(roots,supports,native,nodes):
    keep=np.ones(len(roots),dtype=bool);pending={}
    for index,triangle in enumerate(roots):
        xyz=np.asarray([nodes.coordinates[int(root)] for root in triangle])
        if not zero_area(xyz):continue
        key=(int(native[index]),supports[index],cycle(triangle))
        opposite=(key[0],key[1],cycle(triangle[[0,2,1]]))
        candidates=pending.get(opposite)
        if candidates:
            previous=candidates.pop();keep[previous]=keep[index]=False
        else:pending.setdefault(key,[]).append(index)
    return roots[keep],tuple(s for s,k in zip(supports,keep,strict=True) if k),np.asarray(native,dtype=np.int32)[keep]
