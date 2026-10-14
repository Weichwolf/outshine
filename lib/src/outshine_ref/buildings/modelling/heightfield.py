"""Closed ENU envelopes below an oriented, non-overlapping triangular heightfield."""

from dataclasses import dataclass
from numbers import Integral
from types import MappingProxyType

import numpy as np
import shapely
from shapely.affinity import translate

from .mesh_fans import connected_fans
from .prism_union import frozen
from .scope import scalar


@dataclass(frozen=True)
class HeightfieldEnvelope:
    mesh: object
    vertex_roots: object
    source_triangles: object
    surface_roles: object
    boundary_edges: tuple
    footprint_wkb: bytes
    base_m: float
    projection_area_m2: float
    volume_m3: float


def material(value):
    if isinstance(value,bool) or not isinstance(value,Integral) or not 0<=value<=65535:
        raise ValueError('Envelope materials require unsigned 16-bit identifiers')
    return int(value)


def source_mesh(mesh,base):
    allowed={'vertices','faces','materials','parts','groups','face_tag_mask','normals','uv'}|{'uv'+str(i) for i in range(1,10)}
    if set(mesh)-allowed:raise NotImplementedError('Unsupported heightfield vertex or face channel')
    vertices=np.asarray(mesh['vertices'],dtype=np.float64);faces=np.asarray(mesh['faces'])
    if vertices.ndim!=2 or vertices.shape[1]!=3 or not np.isfinite(vertices).all():
        raise ValueError('Heightfield vertices require finite ENU coordinates')
    if faces.ndim!=2 or faces.shape[1]!=3 or faces.dtype.kind not in 'iu' or not len(faces):
        raise ValueError('Heightfield needs nonempty integer triangle indices')
    if faces.min()<0 or faces.max()>=len(vertices):raise ValueError('Heightfield triangle references a missing vertex')
    triangles=vertices[faces];u,v=triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]
    projected=u[:,0]*v[:,1]-u[:,1]*v[:,0]
    uncertainty=8*np.finfo(float).eps*(abs(u[:,0]*v[:,1])+abs(u[:,1]*v[:,0]))
    if np.any(projected<=uncertainty):raise ValueError('Heightfield triangles must face upward with resolved positive projected area')
    if np.any(triangles[:,:,2]<=base):raise ValueError('Envelope base must lie strictly below its complete upper surface')
    channels={}
    for key,value in mesh.items():
        if key=='uv' or key.startswith('uv'):
            values=np.asarray(value,dtype=np.float64)
            if values.shape!=(len(vertices),2) or not np.isfinite(values).all():raise ValueError('Heightfield UV channels require finite corner pairs')
            channels[key]=values[faces].reshape(-1,2)
    channels.setdefault('uv',np.zeros((faces.size,2)))
    fields={}
    for key,dtype,maximum in (('materials',np.uint16,65535),('parts',np.int32,2**31-1),('groups',np.uint32,2**32-1),('face_tag_mask',np.uint8,255)):
        values=np.asarray(mesh.get(key,np.zeros(len(faces),dtype=dtype)))
        if values.shape!=(len(faces),) or values.dtype.kind not in 'iu' or np.any(values>maximum) or np.any(values<0):
            raise ValueError('Invalid heightfield face channel: '+key)
        fields[key]=values.astype(dtype)
    normals=np.cross(u,v);normals/=np.linalg.norm(normals,axis=1,keepdims=True)
    if 'normals' in mesh:
        normals=np.asarray(mesh['normals'],dtype=np.float64)
        if normals.shape!=vertices.shape or not np.isfinite(normals).all() or np.any(np.linalg.norm(normals[faces],axis=2)==0):
            raise ValueError('Invalid authored heightfield normals')
        normals=normals[faces].reshape(-1,3)
    else:normals=np.repeat(normals,3,axis=0)
    return triangles.reshape(-1,3),channels,fields,normals,projected/2


def topology(points,uv,areas):
    cached=dict(vertices=points,faces=np.arange(len(points)).reshape(-1,3),uv=uv)
    _,triangles,duplicates,targets=connected_fans(cached,np.arange(len(cached['faces'])))
    roots=np.arange(len(points),dtype=np.uint64);roots[duplicates]=targets
    boundary={}
    for number,triangle in enumerate(triangles):
        for a,b in ((triangle[0],triangle[1]),(triangle[1],triangle[2]),(triangle[2],triangle[0])):
            key=int(roots[a]),int(roots[b])
            if key[::-1] in boundary:del boundary[key[::-1]]
            elif key in boundary:raise ValueError('Heightfield has duplicate directed boundaries')
            else:boundary[key]=(int(a),int(b),number)
    origin=(points[:,:2].min(axis=0)+points[:,:2].max(axis=0))/2
    projected=points[:,:2]-origin
    polygons=shapely.polygons(projected.reshape(-1,3,2))
    region=shapely.union_all(polygons)
    if region.geom_type=='GeometryCollection':
        region=shapely.union_all([part for part in shapely.get_parts(region) if part.geom_type in ('Polygon','MultiPolygon')])
    if region.is_empty or not region.is_valid or abs(float(areas.sum())-region.area)>1e-8:
        raise ValueError('Heightfield projections overlap or have an invalid domain')
    for a,b,_ in boundary.values():
        line=shapely.LineString(projected[[a,b]])
        if line.difference(region.boundary).length>1e-8:
            raise ValueError('Heightfield has an unmatched interior edge or T junction')
    return roots,tuple(boundary.values()),translate(region,*origin)


def assemble(points,channels,fields,normals,roots,edges,base,wall,lower,scale):
    count=len(points);triangles=count//3;bottom=points.reshape(-1,3,3)[:,::-1].reshape(-1,3).copy();bottom[:,2]=base
    top_roots=roots.reshape(-1,3);bottom_roots=top_roots[:,::-1].reshape(-1)+count
    xy_origin=points[:,:2].min(axis=0);bottom_uv=(bottom[:,:2]-xy_origin)/scale
    vertices=[points,bottom];identities=[roots,bottom_roots];normal_rows=[normals,np.tile([0.,0.,-1.],(count,1))]
    uv_rows={key:[values,bottom_uv] for key,values in channels.items()}
    face_rows={key:[values,values.copy()] for key,values in fields.items()}
    face_rows['materials'][1]=np.full(triangles,lower,dtype=np.uint16)
    face_rows['face_tag_mask'][1]=np.zeros(triangles,dtype=np.uint8)
    source=[np.arange(triangles),np.arange(triangles)];roles=[np.zeros(triangles,dtype=np.uint8),np.ones(triangles,dtype=np.uint8)]
    for a,b,owner in edges:
        upper=points[[b,a,a,b]].copy();upper[2:,2]=base;order=np.array([0,1,2,0,2,3]);quad=upper[order]
        vertex_ids=np.array([roots[b],roots[a],roots[a]+count,roots[b]+count],dtype=np.uint64)[order]
        normal=np.cross(quad[1]-quad[0],quad[2]-quad[0]);normal/=np.linalg.norm(normal)
        width=np.linalg.norm(points[b,:2]-points[a,:2]);uv=np.array([[width,points[b,2]-base],[0.,points[a,2]-base],[0.,0.],[width,0.]])/scale
        vertices.append(quad);identities.append(vertex_ids);normal_rows.append(np.tile(normal,(6,1)))
        for key in uv_rows:uv_rows[key].append(uv[order])
        for key in face_rows:face_rows[key].append(np.full(2,wall if key=='materials' else 0 if key=='face_tag_mask' else fields[key][owner],dtype=fields[key].dtype))
        source.append(np.full(2,owner));roles.append(np.full(2,2,dtype=np.uint8))
    vertices=np.concatenate(vertices)
    if len(vertices)>=2**32:raise ValueError('Envelope exceeds its unsigned 32-bit corner index range')
    mesh=dict(vertices=vertices,faces=np.arange(len(vertices),dtype=np.uint32).reshape(-1,3),normals=np.concatenate(normal_rows))
    mesh.update({key:np.concatenate(values) for key,values in uv_rows.items()})
    mesh.update({key:np.concatenate(values) for key,values in face_rows.items()})
    return MappingProxyType({key:frozen(value) for key,value in mesh.items()}),frozen(np.concatenate(identities)),frozen(np.concatenate(source)),frozen(np.concatenate(roles))


def heightfield_envelope(top,base_m,wall_material=1,base_material=0,wall_uv_scale_m=(1.,1.)):
    """Close a native upward ENU top surface using actual boundary walls and one lower plane.

    Triangle coverage may comprise separate domains or point contacts. Edge-connected fans
    remain explicit; point contacts never weld. Upper corner streams are retained exactly.
    UV0–9 on new lower/wall surfaces use declared metric projection; this primitive does
    not inherit arbitrary CGA derivation tags, material tables or a native asset-cache ABI.
    """
    base=scalar(base_m);wall,lower=material(wall_material),material(base_material)
    scale=np.asarray(wall_uv_scale_m,dtype=float)
    if scale.shape!=(2,) or not np.isfinite(scale).all() or np.any(scale<=0):raise ValueError('Envelope UV scales require two positive metre distances')
    points,channels,fields,normals,areas=source_mesh(top,base)
    roots,edges,region=topology(points,channels['uv'],areas)
    mesh,roots,source,roles=assemble(points,channels,fields,normals,roots,edges,base,wall,lower,scale)
    volume=float(np.sum(areas*(points.reshape(-1,3,3)[:,:,2].mean(axis=1)-base)))
    return HeightfieldEnvelope(mesh,roots,source,roles,edges,region.wkb,base,region.area,volume)
