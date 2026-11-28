"""Closed indexed radial solids, explicit topology roots and metric UV/normal bindings."""

from dataclasses import dataclass
import math
from numbers import Integral
from types import MappingProxyType

import numpy as np

from .radial_profile import RadialProfile, detail_profile, radial_error
from .prism_union import frozen


@dataclass(frozen=True)
class RadialMesh:
    mesh: object
    vertex_roots: object
    profile_indices: tuple
    sides: int
    error_m: float
    profile_error_m: float
    angular_error_m: float
    identifier: str
    volume_m3: float
    area_m2: float


def topology(profile,indices,sides):
    points=[];faces=[];roles=[];bands=[];rings={};values=np.asarray(profile.knots)[list(indices)]
    angles=np.arange(sides)*2*math.pi/sides;directions=np.column_stack((np.cos(angles),np.sin(angles)))
    for role in range(2 if profile.hollow else 1):
        for level,(height,outer,inner) in enumerate(values):
            radius=(outer,inner)[role];first=len(points)
            xy=np.asarray(profile.centre_xy)+radius*directions if radius else np.asarray(profile.centre_xy)[None,:]
            points.extend(np.c_[xy,np.full(len(xy),height)].tolist());rings[role,level]=list(range(first,len(points)))
        for level in range(len(values)-1):
            low,high=rings[role,level],rings[role,level+1]
            for i in range(sides):
                j=(i+1)%sides
                triangles=([(low[0],high[j],high[i])] if len(low)==1 else
                           [(low[i],low[j],high[0])] if len(high)==1 else
                           [(low[i],low[j],high[j]),(low[i],high[j],high[i])])
                for triangle in triangles:
                    faces.append(triangle[::-1] if role else triangle);roles.append(role);bands.append(level)
    for level,up in ((0,False),(len(values)-1,True)):
        outer=rings[0,level]
        if len(outer)==1:continue
        inner=rings.get((1,level))
        if inner is None:
            centre=len(points);points.append([*profile.centre_xy,values[level,0]])
        for i in range(sides):
            j=(i+1)%sides
            triangles=[(outer[i],outer[j],inner[j]),(outer[i],inner[j],inner[i])] if inner is not None else [(outer[i],outer[j],centre)]
            for triangle in triangles:
                faces.append(triangle if up else triangle[::-1]);roles.append(2 if up else 3);bands.append(level)
    return np.asarray(points),np.asarray(faces,dtype=np.uint32),np.asarray(roles,dtype=np.uint8),np.asarray(bands),values


def corner_data(profile,points,faces,roles,bands,values):
    corners=points[faces];xy=corners[:,:,:2]-profile.centre_xy
    theta=np.arctan2(xy[:,:,1],xy[:,:,0]);theta=np.where(theta<0,theta+2*math.pi,theta)
    pole=np.linalg.norm(xy,axis=2)==0
    maximum=np.max(np.where(pole,-math.inf,theta),axis=1)
    minimum=np.min(np.where(pole,math.inf,theta),axis=1)
    seam=maximum-minimum>math.pi
    theta=np.where(seam[:,None] & (theta<math.pi),theta+2*math.pi,theta)
    normals=np.zeros_like(corners);uv=np.zeros((len(faces),3,2))
    for role in (0,1):
        selected=roles==role
        if not np.any(selected):continue
        current=bands[selected];dr=np.diff(values[:,role+1]);dz=np.diff(values[:,0])
        angles=theta[selected].copy();pole=np.linalg.norm(xy[selected],axis=2)==0
        average=np.sum(np.where(pole,0.,angles),axis=1)/np.sum(~pole,axis=1)
        angles=np.where(pole,average[:,None],angles)
        n=np.stack((np.cos(angles),np.sin(angles),np.broadcast_to((-dr/dz)[current,None],angles.shape)),axis=2)
        n/=np.linalg.norm(n,axis=2)[:,:,None];normals[selected]=n*(-1 if role else 1)
        uv[selected,:,0]=angles/(2*math.pi)
        uv[selected,:,1]=(corners[selected,:,2]-values[0,0])/(values[-1,0]-values[0,0])
    for role,sign in ((2,1),(3,-1)):
        selected=roles==role;normals[selected,:,2]=sign
        radius=np.max(values[:,1]);uv[selected]=xy[selected]/(2*radius)+.5
    return corners,normals,uv


def radial_mesh(profile,*,sides=32,error_m=None,max_sides=256,max_triangles=262144):
    if not isinstance(profile,RadialProfile):raise ValueError('Radial mesh needs a complete RadialProfile')
    if isinstance(max_triangles,bool) or not isinstance(max_triangles,Integral) or max_triangles<1:
        raise ValueError('Radial mesh needs a positive triangle cap')
    if isinstance(max_sides,bool) or not isinstance(max_sides,Integral) or max_sides<3:
        raise ValueError('Radial mesh needs an integer angular side cap of at least three')
    if error_m is None:
        if isinstance(sides,bool) or not isinstance(sides,Integral) or sides<3 or sides>max_sides:
            raise ValueError('Radial mesh side count outside the declared cap')
        indices=tuple(range(len(profile.knots)))
    else:
        indices,sides=detail_profile(profile,error_m,max_sides)
    radii=np.asarray(profile.knots)[list(indices),1]
    bound=(4*sides*(len(indices)-1) if profile.hollow else
           sides*sum(1 if a==0 or b==0 else 2 for a,b in zip(radii[:-1],radii[1:],strict=True)))
    bound+=(2 if profile.hollow else 1)*sides*sum(r>0 for r in (radii[0],radii[-1]))
    if bound>max_triangles:raise ValueError('Radial mesh triangle cap exceeded before emission')
    points,faces,roles,bands,values=topology(profile,indices,sides)
    return finish(profile,points,faces,roles,bands,values,indices,sides)


def finish(profile,points,faces,roles,bands,values,indices,sides):
    corners=points[faces]
    cross=np.cross(corners[:,1]-corners[:,0],corners[:,2]-corners[:,0]);length=np.linalg.norm(cross,axis=1)
    if not np.isfinite(corners).all() or not np.isfinite(length).all() or np.any(length==0):
        raise ValueError('Radial mesh has unresolved or nonfinite geometry')
    corners,normals,uv=corner_data(profile,points,faces,roles,bands,values)
    if not np.isfinite(normals).all():raise ValueError('Radial mesh has nonfinite normals')
    local=corners-np.r_[profile.centre_xy,values[0,0]]
    volume=float(np.sum(np.einsum('ij,ij->i',local[:,0],np.cross(local[:,1],local[:,2])))/6)
    materials=np.take(profile.materials,np.minimum(roles,2)).astype(np.uint16)
    mesh=MappingProxyType(dict(vertices=frozen(corners.reshape(-1,3)),faces=frozen(np.arange(len(faces)*3,dtype=np.uint32).reshape(-1,3)),
        normals=frozen(normals.reshape(-1,3)),uv=frozen(uv.reshape(-1,2)),materials=frozen(materials),parts=frozen(roles)))
    error,vertical,angular=radial_error(profile,indices,sides)
    return RadialMesh(mesh,frozen(faces.reshape(-1)),indices,int(sides),error,vertical,angular,
                      profile.identifier,volume,float(np.sum(length)/2))
