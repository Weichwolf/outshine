"""Projected barycentric construction and independent exact-rational reference."""

from fractions import Fraction

import numpy as np


def axes_for(corners):
    normal=np.cross(corners[1]-corners[0],corners[2]-corners[0])
    return [i for i in range(3) if i!=int(np.argmax(abs(normal)))]


def projected_weights(corners,points):
    axes=axes_for(corners)
    a,b,c=corners[:,axes].astype(np.longdouble)
    u=b-a;v=c-a;q=points[:,axes].astype(np.longdouble)-a
    determinant=u[0]*v[1]-u[1]*v[0]
    first=(q[:,0]*v[1]-q[:,1]*v[0])/determinant
    last=(u[0]*q[:,1]-u[1]*q[:,0])/determinant
    return np.asarray(np.c_[1-first-last,first,last],dtype=float)


def rational_weights(corners,points):
    axes=axes_for(corners)
    a,b,c=[tuple(Fraction(float(value)) for value in p[axes]) for p in corners]
    u=tuple(y-x for x,y in zip(a,b,strict=True));v=tuple(y-x for x,y in zip(a,c,strict=True))
    determinant=u[0]*v[1]-u[1]*v[0];result=[]
    for point in points:
        q=tuple(Fraction(float(value))-start for value,start in zip(point[axes],a,strict=True))
        first=(q[0]*v[1]-q[1]*v[0])/determinant
        last=(u[0]*q[1]-u[1]*q[0])/determinant
        result.append((float(1-first-last),float(first),float(last)))
    return np.asarray(result,dtype=float)
