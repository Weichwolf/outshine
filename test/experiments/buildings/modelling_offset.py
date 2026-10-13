"""Offset logical planar faces in their actual metric planes and preserve affine UV channels."""

from dataclasses import dataclass,replace

import numpy as np

from modelling_component_geometry import fit_scope
from modelling_faces import face_shape,plane
from modelling_metadata import inherit
from modelling_offset_plan import offset as plan_offset
from outshine_ref.buildings.modelling.scope import scalar
from modelling_uv import corner_values


@dataclass(frozen=True)
class OffsetInfo:
    source_faces: tuple
    regions: tuple
    occluder: object=None


def affine_values(coordinates,values):
    xy=np.concatenate(coordinates);data=np.concatenate(values)
    design=np.c_[xy,np.ones(len(xy))];matrix,_,rank,_=np.linalg.lstsq(design,data,rcond=None)
    if rank!=3 or not np.allclose(design@matrix,data,atol=1e-8,rtol=1e-10):
        raise NotImplementedError('Offset requires affine corner attributes; nonaffine extrapolation remains pending')
    return matrix


def append_regions(vertices,face,number,fields,distance,keep,points,faces,owners,kinds,known):
    _,polygon,coordinates,face=plane(vertices,face)
    rings=(face.outer,*face.holes)
    world=affine_values(coordinates,[vertices[list(ring)] for ring in rings])
    channels={}
    for key,stream in fields.items():
        if key=='uv' or key.startswith('uv') and key[2:].isdigit():
            index=0 if key=='uv' else int(key[2:])
            channels[index]=affine_values(coordinates,corner_values(face,index,stream))
    original={tuple(pair):index for ring,xy in zip(rings,coordinates,strict=True) for index,pair in zip(ring,xy,strict=True)}
    for region in plan_offset(polygon,distance,keep).regions:
        ring_indices=[];uv={channel:[] for channel in channels}
        for ring in (region.polygon.exterior,*region.polygon.interiors):
            xy=np.asarray(ring.coords)[:-1];design=np.c_[xy,np.ones(len(xy))];indices=[]
            for pair,point in zip(xy,design@world,strict=True):
                key=('source',original[tuple(pair)]) if tuple(pair) in original else (number,*pair)
                if key not in known:known[key]=len(points);points.append(point)
                indices.append(known[key])
            ring_indices.append(tuple(indices))
            for channel,matrix in channels.items():uv[channel].append(tuple(map(tuple,design@matrix)))
        faces.append(replace(face,outer=ring_indices[0],holes=tuple(ring_indices[1:]),
            uvs=tuple(uv.get(0,())),uv_sets=tuple((channel,tuple(value)) for channel,value in uv.items() if channel)))
        owners.append(number);kinds.append(region.kind)


def offset(shape,distance,keep=None):
    distance=scalar(distance)
    if keep not in (None,'all','inside','border'):raise ValueError('Unknown offset selector')
    geometry=shape.geometry
    if geometry.edges or geometry.points or not geometry.polygons:
        raise NotImplementedError('Offset needs logical polygon faces without wire or point primitives')
    allowed={'faces','parts','materials','groups','face_tag_mask','uv'}|{'uv'+str(index) for index in range(1,10)}
    if set(geometry.fields)-allowed:raise NotImplementedError('Offset authored normals or other vertex fields remain pending')
    emitted=shape.emit();points=[];faces=[];owners=[];kinds=[];known={}
    for number,face in enumerate(geometry.polygons):
        append_regions(emitted['vertices'],face,number,emitted,distance,keep,points,faces,owners,kinds,known)
    vertices=np.asarray(points,dtype=float).reshape(-1,3)
    scope=fit_scope(vertices,shape.scope.axes,shape.scope.origin,shape.scope,signed=True) if len(vertices) else replace(shape.scope,size=(0.,0.,0.))
    result=inherit(face_shape(vertices,faces,scope=scope),shape)
    if 'uv' not in geometry.fields:result=result.delete_uv(0)
    return replace(result,comp_info=shape.comp_info,
        offset_info=OffsetInfo(tuple(owners),tuple(kinds),shape if keep is not None else None))
