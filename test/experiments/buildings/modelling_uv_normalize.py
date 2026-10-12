"""Normalize active UVs collectively or give logical faces independent coordinate entries."""

from dataclasses import replace
from types import MappingProxyType
import warnings

import numpy as np

from modelling_channels import vertex_channel
from modelling_component_geometry import topology
from modelling_uv import channel,name,corner_values,face_channel,affine


UV_ZERO=1e-25
MODES=('uv','uvUniform','u','v')
TYPES=('separatePerFace','collectiveAllFaces')


def transform(values,mode):
    low=values.min(axis=0);span=values.max(axis=0)-low
    active=np.array([mode!='v',mode!='u'])
    if np.any(span[active]<UV_ZERO):return None
    scale=np.ones(2);offset=np.zeros(2)
    scale[active]=1./(max(span) if mode=='uvUniform' else span[active])
    offset[active]=-low[active]*scale[active]
    return np.diag(scale),offset


def coordinates(stream,used,face,index):
    values=[stream[used]]
    corner=corner_values(face,index,stream)
    if corner:values.extend(np.asarray(row) for row in corner)
    return np.concatenate(values)


def per_face(shape,index,mode):
    geometry=shape.geometry;fields=geometry.fields;faces,rows=topology(shape,fields);stream=fields[name(index)]
    sources=[];triangles=np.empty_like(fields['faces']);polygons=[];uv=[];offset=0
    for face,triangle_rows in zip(faces,rows,strict=True):
        subset=fields['faces'][list(triangle_rows)]
        used=np.unique(np.r_[subset.ravel(),*[np.asarray(ring) for ring in (face.outer,*face.holes)]]).astype(np.int64)
        matrix=transform(coordinates(stream,used,face,index),mode)
        matrix,translation=(np.eye(2),np.zeros(2)) if matrix is None else matrix
        values=corner_values(face,index,stream)
        transformed=tuple(tuple(tuple(pair) for pair in np.asarray(row)@matrix.T+translation) for row in values)
        updated=face_channel(face,index,transformed)
        mapping={int(old):int(new)+offset for new,old in enumerate(used)}
        polygons.append(replace(updated,outer=tuple(mapping[i] for i in face.outer),holes=tuple(tuple(mapping[i] for i in ring) for ring in face.holes)))
        triangles[list(triangle_rows)]=np.searchsorted(used,subset)+offset
        sources.append(used);uv.append(stream[used]@matrix.T+translation);offset+=len(used)
    used=np.concatenate(sources) if sources else np.empty(0,dtype=np.int64)
    result={key:array[used] if vertex_channel(key) else array for key,array in fields.items()}
    result['faces']=triangles;result[name(index)]=np.concatenate(uv) if uv else np.empty((0,2))
    result.setdefault('parts',np.concatenate([np.full(len(row),i,dtype=np.int32) for i,row in enumerate(rows)]))
    for value in result.values():value.setflags(write=False)
    units=geometry.unit_vertices[used];units.setflags(write=False)
    return replace(shape,geometry=replace(geometry,fields=MappingProxyType(result),unit_vertices=units,polygons=tuple(polygons)))


def normalize_uv(shape,index,mode='uv',kind='collectiveAllFaces'):
    index=channel(index)
    if mode not in MODES or kind not in TYPES:raise ValueError('Invalid UV normalization mode or type')
    geometry=shape.geometry
    if geometry.edges or geometry.points:raise NotImplementedError('Mixed wire/surface UV normalization remains pending')
    stream=geometry.fields.get(name(index))
    if stream is None or not len(geometry.fields['faces']):return shape
    if kind=='separatePerFace':return per_face(shape,index,mode)
    used=np.unique(geometry.fields['faces']);values=[stream[used]]
    for face in geometry.polygons:
        rows=corner_values(face,index,stream)
        if rows:values.extend(np.asarray(row) for row in rows)
    matrix=transform(np.concatenate(values),mode)
    if matrix is None:
        warnings.warn('Collective UV normalization leaves degenerate coordinates unchanged',RuntimeWarning,stacklevel=2)
        return shape
    return affine(shape,index,*matrix)
