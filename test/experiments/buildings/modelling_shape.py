"""Owned immutable triangle assets and deferred scope placement, without recipe serialization."""

from dataclasses import dataclass,replace
from types import MappingProxyType

import numpy as np

from modelling_scope import Scope


@dataclass(frozen=True)
class CompInfo:
    sel: str
    index: int
    total: int
    source_faces: tuple


@dataclass(frozen=True)
class Geometry:
    fields: object
    unit_vertices: object
    reference_size: tuple
    polygons: tuple=()

    @classmethod
    def from_mesh(cls,mesh,scope=None,polygons=()):
        vertices=np.asarray(mesh['vertices'],dtype=np.float64)
        raw_faces=np.asarray(mesh['faces'])
        if vertices.ndim!=2 or vertices.shape[1]!=3 or not np.isfinite(vertices).all():
            raise ValueError('Shape vertices require finite XYZ coordinates')
        if raw_faces.ndim!=2 or raw_faces.shape[1]!=3 or raw_faces.dtype.kind not in 'iu':
            raise ValueError('This mesh constructor requires triangle indices')
        if len(raw_faces) and (raw_faces.min()<0 or raw_faces.max()>=len(vertices)):
            raise ValueError('Shape face index lies outside its vertices')
        if scope is None:
            low=vertices.min(axis=0) if len(vertices) else np.zeros(3)
            size=vertices.max(axis=0)-low if len(vertices) else np.zeros(3)
            scope=Scope(position=tuple(low),size=tuple(size))
        local=(vertices-scope.origin)@scope.axes
        size=np.asarray(scope.size)
        if np.any(np.abs(local[:,size==0])>1e-8):raise ValueError('Geometry does not fit its zero-sized scope axes')
        unit=np.divide(local,size,out=np.zeros_like(vertices),where=size!=0)
        unit.setflags(write=False)
        fields={}
        for key,value in mesh.items():
            if key=='vertices':continue
            array=np.array(value,copy=True);array.setflags(write=False);fields[key]=array
        if 'normals' in fields and (fields['normals'].shape!=vertices.shape or not np.isfinite(fields['normals']).all()):
            raise ValueError('Authored vertex normals require finite XYZ coordinates per vertex')
        if 'normals' in fields:
            normals=fields['normals']@scope.axes;normals.setflags(write=False);fields['normals']=normals
        return cls(MappingProxyType(fields),unit,tuple(size),tuple(polygons)),scope


@dataclass(frozen=True)
class Shape:
    geometry: Geometry
    scope: Scope
    comp_info: object=None

    @classmethod
    def from_mesh(cls,mesh,scope=None,polygons=()):
        geometry,scope=Geometry.from_mesh(mesh,scope,polygons)
        return cls(geometry,scope)

    @classmethod
    def from_polygon(cls,polygon,elevation=0.,material=1,tags=()):
        from modelling_faces import polygon_shape
        return polygon_shape(polygon,elevation,material,tags)

    @classmethod
    def from_faces(cls,vertices,faces,scope=None,vertex_normals=None):
        from modelling_faces import face_shape
        return face_shape(vertices,faces,scope,vertex_normals)

    def extrude(self,distance,mode='face.normal'):
        from modelling_extrude import extrude
        return extrude(self,distance,mode)

    def comp(self,component,*branches,alignment='zUp'):
        from modelling_components import split_components
        return split_components(self,component,branches,alignment)

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
