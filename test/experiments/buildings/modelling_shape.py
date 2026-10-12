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
    source_indices: tuple
    kind: str='f'


@dataclass(frozen=True)
class Geometry:
    fields: object
    unit_vertices: object
    reference_size: tuple
    polygons: tuple=()
    edges: tuple=()
    points: tuple=()
    group_names: tuple=()
    material_defs: tuple=()

    @classmethod
    def from_mesh(cls,mesh,scope=None,polygons=(),edges=(),points=(),group_names=(),material_defs=()):
        from modelling_metadata import named_groups,materials,validate_membership
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
        if 'materials' in fields:
            values=fields['materials']
            if values.shape!=(len(raw_faces),) or values.dtype.kind not in 'iu' or (len(values) and (values.min()<0 or values.max()>65535)):
                raise ValueError('Native materials require one unsigned 16-bit identifier per triangle')
        if 'groups' in fields:
            groups=fields['groups']
            if groups.shape!=(len(raw_faces),) or groups.dtype.kind not in 'iu' or (len(groups) and (groups.min()<0 or groups.max()>4294967295)):
                raise ValueError('Native groups require one unsigned 32-bit identifier per triangle')
        for name in ('edges','points'):
            if name not in fields:continue
            indices=fields[name];width=2 if name=='edges' else 1
            if indices.ndim!=2 or indices.shape[1]!=width or indices.dtype.kind not in 'iu' or (len(indices) and (indices.min()<0 or indices.max()>=len(vertices))):
                raise ValueError('Invalid native '+name+' primitive indices')
            normal=fields.get(name+'_normals')
            if normal is not None and (normal.shape!=(len(indices),3) or not np.isfinite(normal).all()):
                raise ValueError('Invalid native '+name+' primitive normals')
            groups=fields.get(name+'_groups')
            if groups is not None and (groups.shape!=(len(indices),) or groups.dtype.kind not in 'iu' or (len(groups) and (groups.min()<0 or groups.max()>4294967295))):
                raise ValueError('Invalid native '+name+' group identifiers')
            material=fields.get(name+'_materials')
            if material is not None and (material.shape!=(len(indices),) or material.dtype.kind not in 'iu' or (len(material) and (material.min()<0 or material.max()>65535))):
                raise ValueError('Invalid native '+name+' material identifiers')
        for name in ('normals','edges_normals','points_normals'):
            if name in fields:
                normals=fields[name]@scope.axes;normals.setflags(write=False);fields[name]=normals
        validate_membership(fields,polygons,edges,points,len(raw_faces))
        return cls(MappingProxyType(fields),unit,tuple(size),tuple(polygons),tuple(edges),tuple(points),named_groups(group_names),materials(material_defs)),scope


@dataclass(frozen=True)
class Shape:
    geometry: Geometry
    scope: Scope
    comp_info: object=None
    sweep_info: object=None

    @classmethod
    def from_mesh(cls,mesh,scope=None,polygons=(),edges=(),points=(),group_names=(),material_defs=()):
        geometry,scope=Geometry.from_mesh(mesh,scope,polygons,edges,points,group_names,material_defs)
        return cls(geometry,scope)

    @classmethod
    def from_polygon(cls,polygon,elevation=0.,material=1,tags=()):
        from modelling_faces import polygon_shape
        return polygon_shape(polygon,elevation,material,tags)

    @classmethod
    def from_faces(cls,vertices,faces,scope=None,vertex_normals=None):
        from modelling_faces import face_shape
        return face_shape(vertices,faces,scope,vertex_normals)

    @classmethod
    def from_edges(cls,vertices,edges,normals,scope=None):
        from modelling_wire import wire_shape
        return wire_shape(vertices,edges,normals,scope)

    @classmethod
    def from_points(cls,vertices,points,normals,scope=None):
        from modelling_wire import wire_shape
        return wire_shape(vertices,points,normals,scope)

    def extrude(self,distance,mode='face.normal'):
        from modelling_extrude import extrude
        return extrude(self,distance,mode)

    def comp(self,component,*branches,alignment='zUp'):
        from modelling_components import split_components
        return split_components(self,component,branches,alignment)

    def with_metadata(self,group_names=(),material_defs=()):
        from modelling_metadata import named_groups,materials
        return replace(self,geometry=replace(self.geometry,group_names=named_groups(group_names),material_defs=materials(material_defs)))

    def group_names(self):
        from modelling_group_components import names
        return names(self,'g')

    def material_names(self):
        from modelling_group_components import names
        return names(self,'m')

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
        for name in ('normals','edges_normals','points_normals'):
            if name not in result:continue
            scale=np.divide(self.scope.size,self.geometry.reference_size,out=np.ones(3),
                            where=np.asarray(self.geometry.reference_size)!=0)
            if np.any(scale==0):raise ValueError('Singular authored-normal transforms need a verified cleanup contract')
            normals=result[name]@(np.diag(1/scale)@self.scope.axes.T)
            length=np.linalg.norm(normals,axis=1)
            result[name]=np.divide(normals,length[:,None],out=np.zeros_like(normals),where=length[:,None]!=0)
        return result
