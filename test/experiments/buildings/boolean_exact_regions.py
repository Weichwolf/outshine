"""Rational convex clipping; exact binary input coordinates, no perturbation."""

from fractions import Fraction

import numpy as np


def point(values):
    return tuple(Fraction(float(value)) for value in values)


def orientation(a, b, c):
    return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])


def area(polygon):
    return sum(orientation(polygon[0], polygon[i], polygon[i+1])
               for i in range(1, len(polygon)-1))/2 if len(polygon)>2 else Fraction(0)


def clean(polygon):
    result=tuple(p for i,p in enumerate(polygon) if p!=polygon[i-1])
    if any(max(v.numerator.bit_length(),v.denominator.bit_length())>4096 for p in result for v in p):
        raise ValueError('Exact construction exceeds rational bit capacity')
    return result if area(result)!=0 else ()


def clip(polygon, a, b, sign):
    result=[]
    for i,p in enumerate(polygon):
        q=polygon[i-1];dp=sign*orientation(a,b,p);dq=sign*orientation(a,b,q)
        if (dp<0)!=(dq<0):
            ratio=dq/(dq-dp)
            result.append(tuple(q[j]+ratio*(p[j]-q[j]) for j in range(2)))
        if dp>=0:result.append(p)
    return clean(result)


def partition(polygon, triangle):
    sign=1 if area(triangle)>0 else -1
    remaining=polygon;outside=[]
    for i,a in enumerate(triangle):
        if not remaining:break
        b=triangle[(i+1)%3]
        piece=clip(remaining,a,b,-sign)
        if piece:outside.append(piece)
        remaining=clip(remaining,a,b,sign)
    if sum(abs(area(p)) for p in outside)+abs(area(remaining))!=abs(area(polygon)):
        raise ValueError('Exact convex partition changed area')
    return outside,remaining


def regions(product,nodes,number,sources,max_cells):
    corners,normal,_,axes=nodes.projection(number)
    owner=int(product.operands[number]);source=int(product.source_triangles[number])
    original=clean(tuple(point(p) for p in corners[:,axes]))
    current=[(original,((owner,source),))]
    other=sources[1-owner]
    selected=np.flatnonzero(np.all(abs((other-corners[0])@normal)<=nodes.precision*4,axis=1))
    lower=corners[:,axes].min(axis=0);upper=corners[:,axes].max(axis=0)
    for index in selected:
        candidate=other[index][:,axes]
        if np.any(candidate.max(axis=0)<lower) or np.any(candidate.min(axis=0)>upper):continue
        triangle=tuple(point(p) for p in candidate)
        if area(triangle)==0:continue
        result=[]
        for polygon,support in current:
            if len(support)>1:result.append((polygon,support));continue
            outside,inside=partition(polygon,triangle)
            result.extend((p,support) for p in outside)
            if inside:result.append((inside,tuple(sorted((*support,(1-owner,int(index)))))))
        current=result
        if len(current)>max_cells:raise ValueError('Exact region capacity exceeded')
    if sum(abs(area(p)) for p,_ in current)!=abs(area(original)):
        raise ValueError('Exact regions changed native domain area')
    return tuple(current)
