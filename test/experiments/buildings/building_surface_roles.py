"""Assign exterior wall finish from actual storey boundaries without changing geometry."""

import numpy as np
import shapely

from building_vertical import level


INTERIOR_WALL=12


def apply(plan,product):
    materials=product['materials'].copy();walls=np.flatnonzero(materials==1)
    if np.any(materials==INTERIOR_WALL):raise ValueError('Wall surfaces were already assigned')
    if 'levels' not in product:raise ValueError('Wall finish requires source storey identities')
    if np.any(product['levels'][walls]>=plan['storeys']):raise ValueError('Wall refers to a missing storey')
    triangles=product['vertices'][product['faces'][walls]]
    normals=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])
    lengths=np.linalg.norm(normals,axis=1)
    if np.any(lengths<=1e-12):raise ValueError('Cannot classify a degenerate wall surface')
    normals/=lengths[:,None];centres=triangles.mean(axis=1);exterior=np.zeros(len(walls),dtype=bool)
    for index in range(plan['storeys']):
        selected=np.flatnonzero((product['levels'][walls]==index)&(abs(normals[:,2])<1e-8))
        footprint=level(plan,index)['footprint']
        points=shapely.points(centres[selected,:2])
        outside=shapely.points(centres[selected,:2]+normals[selected,:2]*1e-5)
        exterior[selected]=(shapely.distance(points,footprint.boundary)<1e-7)&~shapely.covers(footprint,outside)
    materials[walls]=np.where(exterior,1,INTERIOR_WALL)
    return product|dict(materials=materials),dict(exterior_triangles=int(exterior.sum()),
        interior_and_reveal_triangles=int((~exterior).sum()),
        exterior_area_m2=float(lengths[exterior].sum()/2),interior_and_reveal_area_m2=float(lengths[~exterior].sum()/2))
