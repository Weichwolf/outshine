"""Immutable UV edits sharing positions, topology, normals and unaffected channels."""

from dataclasses import replace
from numbers import Real
from types import MappingProxyType

import numpy as np

from outshine_ref.buildings.modelling.scope import scalar


def channel(value):
    if isinstance(value,bool) or not isinstance(value,Real) or not np.isfinite(value) or int(value)!=value or not 0<=value<=9:
        raise ValueError('CGA UV set must be an integer in [0,9]')
    return int(value)


def name(index):return 'uv' if index==0 else 'uv'+str(index)


def surface(shape):
    if shape.geometry.edges or shape.geometry.points:raise NotImplementedError('Wire and point UV edits remain pending')


def corner_values(face,index,stream):
    values=face.uvs if index==0 else dict(face.uv_sets).get(index,())
    if values:return values
    return tuple(tuple(tuple(pair) for pair in stream[list(ring)]) for ring in (face.outer,*face.holes)) if stream is not None else ()


def face_channel(face,index,values):
    if index==0:return replace(face,uvs=values)
    rows=dict(face.uv_sets)
    if values:rows[index]=values
    else:rows.pop(index,None)
    return replace(face,uv_sets=tuple(rows.items()))


def changed(shape,index,stream,faces):
    fields=dict(shape.geometry.fields)
    if stream is None:fields.pop(name(index),None)
    else:
        stream=np.asarray(stream,dtype=float)
        if stream.shape!=(len(shape.geometry.unit_vertices),2) or not np.isfinite(stream).all():raise ValueError('Invalid UV result')
        if stream.flags.writeable:stream=stream.copy();stream.setflags(write=False)
        fields[name(index)]=stream
    return replace(shape,geometry=replace(shape.geometry,fields=MappingProxyType(fields),polygons=tuple(faces)))


def affine(shape,index,matrix,offset):
    index=channel(index);surface(shape);stream=shape.geometry.fields.get(name(index))
    if stream is None:return shape
    result=stream@matrix.T+offset
    faces=[face_channel(face,index,tuple(tuple(tuple(pair) for pair in np.asarray(row)@matrix.T+offset)
             for row in corner_values(face,index,stream))) for face in shape.geometry.polygons]
    return changed(shape,index,result,faces)


def scale_uv(shape,index,u,v):return affine(shape,index,np.diag([scalar(u),scalar(v)]),np.zeros(2))


def transform_uv(shape,index,matrix,offset=(0.,0.)):
    matrix=np.asarray(matrix,dtype=float);offset=np.asarray(offset,dtype=float)
    if matrix.shape!=(2,2) or offset.shape!=(2,) or not np.isfinite(matrix).all() or not np.isfinite(offset).all():
        raise ValueError('UV transform requires a finite 2x2 matrix and offset pair')
    return affine(shape,index,matrix,offset)


def translate_uv(shape,index,u,v):return affine(shape,index,np.eye(2),np.array([scalar(u),scalar(v)]))


def rotate_uv(shape,index,angle):
    radians=np.deg2rad(scalar(angle));c,s=np.cos(radians),np.sin(radians)
    return affine(shape,index,np.array([[c,-s],[s,c]]),np.zeros(2))


def copy_uv(shape,source,destination):
    surface(shape)
    source,destination=channel(source),channel(destination)
    if source==destination:return shape
    stream=shape.geometry.fields.get(name(source))
    faces=[face_channel(face,destination,corner_values(face,source,stream) if stream is not None else ()) for face in shape.geometry.polygons]
    return changed(shape,destination,stream,faces)


def delete_uv(shape,index):
    index=channel(index);surface(shape)
    if name(index) not in shape.geometry.fields:return shape
    return changed(shape,index,None,[face_channel(face,index,()) for face in shape.geometry.polygons])


def inherit_primary(result,source):
    geometry=source.geometry
    authored=('uv' in geometry.fields or any(face.uvs for face in geometry.polygons)
              or any(edge.uvs for edge in geometry.edges) or any(point.uvs for point in geometry.points))
    return result if authored or result.geometry.edges or result.geometry.points else delete_uv(result,0)
