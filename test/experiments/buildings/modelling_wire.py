"""Owned line/point primitives with inherited material, tags, UVs and geometric normals."""

from dataclasses import dataclass,replace
from numbers import Integral

import numpy as np

from modelling_scope import scalar
from modelling_metadata import group_id


def identifiers(values,count):
    values=tuple(values)
    if len(values)!=count or any(isinstance(v,bool) or not isinstance(v,Integral) or v<0 for v in values):
        raise ValueError('Wire topology needs nonnegative integer vertex identifiers')
    return tuple(int(value) for value in values)


def attributes(element,uv_count):
    object.__setattr__(element,'group',group_id(element.group))
    if isinstance(element.material,bool) or not isinstance(element.material,Integral) or not 0<=element.material<=65535:
        raise ValueError('Wire material needs an unsigned 16-bit identifier')
    tags=frozenset(element.tags)
    if any(not isinstance(tag,str) for tag in tags):raise ValueError('Wire tags must be strings')
    object.__setattr__(element,'tags',tags)
    if element.uvs:
        uv=tuple(tuple(scalar(value) for value in pair) for pair in element.uvs)
        if len(uv)!=uv_count or any(len(pair)!=2 for pair in uv):raise ValueError('Wire UVs must match its corners')
        object.__setattr__(element,'uvs',uv)


@dataclass(frozen=True)
class Edge:
    vertices: tuple
    material: int=1
    tags: frozenset=frozenset()
    uvs: tuple=()
    group: int=0

    def __post_init__(self):
        object.__setattr__(self,'vertices',identifiers(self.vertices,2));attributes(self,2)
        if self.vertices[0]==self.vertices[1]:raise ValueError('An edge needs distinct vertex identifiers')


@dataclass(frozen=True)
class Point:
    vertices: tuple
    material: int=1
    tags: frozenset=frozenset()
    uvs: tuple=()
    group: int=0

    def __post_init__(self):
        object.__setattr__(self,'vertices',identifiers(self.vertices,1));attributes(self,1)


def wire_shape(vertices,elements,normals,scope=None):
    from modelling_shape import Shape
    elements=tuple(elements);edges=all(isinstance(element,Edge) for element in elements)
    points=all(isinstance(element,Point) for element in elements)
    if not elements or not (edges or points):raise ValueError('A wire constructor needs homogeneous edge or point primitives')
    name='edges' if edges else 'points';width=2 if edges else 1
    indices=np.asarray([element.vertices for element in elements],dtype=np.int32).reshape(-1,width)
    positions=np.asarray(vertices,dtype=np.float64)
    if positions.ndim!=2 or positions.shape[1]!=3 or not np.isfinite(positions).all() or indices.max()>=len(positions):
        raise ValueError('Wire vertices or primitive indices are invalid')
    normals=np.asarray(normals,dtype=np.float64)
    if normals.shape!=(len(elements),3) or not np.isfinite(normals).all() or np.any(np.linalg.norm(normals,axis=1)<=1e-12):
        raise ValueError('Wire primitives need a finite nonzero normal per element')
    normals=normals/np.linalg.norm(normals,axis=1)[:,None]
    if edges and np.any(np.linalg.norm(positions[indices[:,1]]-positions[indices[:,0]],axis=1)<=1e-12):
        raise ValueError('Wire geometry contains a collapsed edge')
    used=np.unique(indices);mapping={int(old):int(new) for new,old in enumerate(used)}
    positions=positions[used];indices=np.searchsorted(used,indices).astype(np.int32)
    elements=tuple(replace(element,vertices=tuple(mapping[i] for i in element.vertices)) for element in elements)
    mesh=dict(vertices=positions,faces=np.empty((0,3),dtype=np.int32),materials=np.empty(0,dtype=np.uint16),parts=np.empty(0,dtype=np.int32),
              **{name:indices,name+'_normals':normals,name+'_materials':np.asarray([element.material for element in elements],dtype=np.uint16),
                 name+'_groups':np.asarray([element.group for element in elements],dtype=np.uint32)})
    return Shape.from_mesh(mesh,scope,edges=elements if edges else (),points=elements if points else ())
