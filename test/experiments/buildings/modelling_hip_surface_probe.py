"""Independent nodal interpolation checks folds inside nonplanar even roof faces."""

from dataclasses import replace

import numpy as np
import shapely

from outshine_ref.buildings.modelling import Frame
from outshine_ref.buildings.modelling.faces import plane


def local(shape):
    return replace(shape,scope=replace(shape.scope,object_frame=Frame())).emit()


def cross(a,b):return a[...,0]*b[...,1]-a[...,1]*b[...,0]


def reference(plan,surface):
    heights=plan.distances_m if surface is None else surface.distances_m
    lookup={tuple(xy):i for i,xy in enumerate(plan.positions_m)}
    triangles=shapely.get_parts(shapely.constrained_delaunay_triangles([f.polygon for f in plan.facets]))
    corners=shapely.get_coordinates(triangles).reshape(-1,4,2)[:,:3]
    usable=cross(corners[:,1]-corners[:,0],corners[:,2]-corners[:,0])!=0
    corners,triangles=corners[usable],triangles[usable]
    values=np.asarray([[heights[lookup[tuple(xy)]] for xy in row] for row in corners])
    return triangles,corners,values


def surface_heights(original,roof,value,mode,mesh=None):
    before=local(original);after=local(roof) if mesh is None else mesh
    top=np.asarray(['roof.top' in f.tags for f in roof.geometry.polygons])[after['parts']]
    samples=np.array([[1.,0.,0.],[0.,1.,0.],[0.,0.,1.],[.2,.3,.5]])
    checked=0;maximum=0.
    for owner,(plan,surface) in enumerate(zip(roof.roof_info.plans,roof.roof_info.surfaces,strict=True)):
        face=original.geometry.polygons[owner];normal,_,_,_=plane(before['vertices'],face)
        origin=before['vertices'][face.outer[0]];vertices=before['vertices'][list(face.outer)]
        edges=np.roll(vertices,-1,axis=0)-vertices;tangent=edges[np.argmax(np.linalg.norm(edges,axis=1))]
        tangent-=normal*(tangent@normal);tangent/=np.linalg.norm(tangent)
        basis=np.array([tangent,np.cross(normal,tangent)])
        selection=top&(np.asarray(roof.roof_info.source_faces)[after['parts']]==owner)
        points=np.einsum('si,tij->tsj',samples,after['vertices'][after['faces'][selection]]).reshape(-1,3)
        projected=(points-origin)@basis.T-plan.origin;actual=(points-origin)@normal
        polys,triangles,values=reference(plan,surface)
        pairs=shapely.STRtree(polys).query(shapely.points(projected),predicate='dwithin',distance=1e-8)
        candidate=triangles[pairs[1]];first,last=candidate[:,1]-candidate[:,0],candidate[:,2]-candidate[:,0]
        delta=projected[pairs[0]]-candidate[:,0];area=cross(first,last)
        u,v=cross(delta,last)/area,cross(first,delta)/area;bary=np.c_[1-u-v,u,v]
        expected=(values[pairs[1]]*bary).sum(axis=1)*(plan if surface is None else surface).factor(value,mode)
        error=abs(expected-actual[pairs[0]])
        valid=(bary.min(axis=1)>=-1e-7)&(error<=1e-7+roof.roof_info.maximum_shared_height_adjustment_m[owner])
        errors=np.full(len(points),np.inf);np.minimum.at(errors,pairs[0][valid],error[valid])
        assert np.isfinite(errors).all(), 'Native hip triangle lost its fixed roof height/fold surface'
        maximum=max(maximum,float(errors.max()));checked+=len(points)
    return dict(height_values_checked=checked,maximum_height_sample_error_m=maximum)
