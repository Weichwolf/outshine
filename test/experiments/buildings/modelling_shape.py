"""Owned immutable triangle assets and deferred scope placement, without recipe serialization."""

from dataclasses import dataclass,replace
from types import MappingProxyType

import numpy as np

from modelling_scope import Scope


@dataclass(frozen=True)
class Geometry:
    fields: object
    unit_vertices: object
    reference_size: tuple

    @classmethod
    def from_mesh(cls,mesh):
        vertices=np.asarray(mesh['vertices'],dtype=np.float64)
        raw_faces=np.asarray(mesh['faces'])
        if vertices.ndim!=2 or vertices.shape[1]!=3 or not np.isfinite(vertices).all():
            raise ValueError('Shape vertices require finite XYZ coordinates')
        if raw_faces.ndim!=2 or raw_faces.shape[1]!=3 or raw_faces.dtype.kind not in 'iu':
            raise ValueError('This mesh constructor requires triangle indices')
        if len(raw_faces) and (raw_faces.min()<0 or raw_faces.max()>=len(vertices)):
            raise ValueError('Shape face index lies outside its vertices')
        low=vertices.min(axis=0) if len(vertices) else np.zeros(3)
        size=vertices.max(axis=0)-low if len(vertices) else np.zeros(3)
        unit=np.divide(vertices-low,size,out=np.zeros_like(vertices),where=size!=0)
        unit.setflags(write=False)
        fields={}
        for key,value in mesh.items():
            if key=='vertices':continue
            array=np.array(value,copy=True);array.setflags(write=False);fields[key]=array
        if 'normals' in fields and (fields['normals'].shape!=vertices.shape or not np.isfinite(fields['normals']).all()):
            raise ValueError('Authored vertex normals require finite XYZ coordinates per vertex')
        return cls(MappingProxyType(fields),unit,tuple(size)),Scope(position=tuple(low),size=tuple(size))


@dataclass(frozen=True)
class Shape:
    geometry: Geometry
    scope: Scope

    @classmethod
    def from_mesh(cls,mesh):
        geometry,scope=Geometry.from_mesh(mesh)
        return cls(geometry,scope)

    def t(self,x,y,z):return replace(self,scope=self.scope.t(x,y,z))

    def r(self,x,y,z,centre='scopeOrigin'):return replace(self,scope=self.scope.r(x,y,z,centre))

    def s(self,x,y,z):return replace(self,scope=self.scope.s(x,y,z))

    def translate(self,mode,coordinates,x,y,z):
        return replace(self,scope=self.scope.translate(mode,coordinates,x,y,z))

    def emit(self):
        matrix=self.scope.matrix
        vertices=self.geometry.unit_vertices@matrix[:3,:3].T+matrix[:3,3]
        if not np.isfinite(vertices).all():raise ValueError('Shape transform produced non-finite coordinates')
        result=dict(self.geometry.fields)|dict(vertices=vertices)
        if 'normals' in result:
            scale=np.divide(self.scope.size,self.geometry.reference_size,out=np.ones(3),
                            where=np.asarray(self.geometry.reference_size)!=0)
            if np.any(scale==0):raise ValueError('Singular authored-normal transforms need a verified cleanup contract')
            normals=result['normals']@(np.diag(1/scale)@self.scope.axes.T)
            length=np.linalg.norm(normals,axis=1)
            result['normals']=np.divide(normals,length[:,None],out=np.zeros_like(normals),where=length[:,None]!=0)
        return result
