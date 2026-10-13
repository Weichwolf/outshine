"""Owned immutable triangle assets and deferred scope placement, without recipe serialization."""

from dataclasses import dataclass,replace
from types import MappingProxyType

import numpy as np

from modelling_scope import Scope
from modelling_metadata import Material


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
        for key,array in fields.items():
            if key=='uv' or (key.startswith('uv') and key[2:].isdigit()):
                if array.shape!=(len(vertices),2) or not np.isfinite(array).all():raise ValueError('UV channels require finite pairs per vertex')
            if key=='vertex_colors':
                if array.ndim!=2 or array.shape[0]!=len(vertices) or array.shape[1] not in (3,4) or not np.isfinite(array).all():
                    raise ValueError('Vertex colours require finite RGB/RGBA per vertex')
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
    split_info: object=None
    asset: object=None
    material: Material=Material()
    textures: tuple=()
    projections: tuple=()
    offset_info: object=None
    roof_info: object=None

    def __post_init__(self):
        if not isinstance(self.material,Material):raise ValueError('Shape material must be an immutable Material')
        from modelling_texture import resources
        object.__setattr__(self,'textures',resources(self.asset.textures if self.asset else (),self.textures))
        from modelling_projection import projections
        object.__setattr__(self,'projections',projections(self.projections))

    def setup_projection(self,index,axes,width,height,width_offset=0.,height_offset=0.,uw_factor=0.):
        from modelling_projection import setup
        return setup(self,index,axes,width,height,width_offset,height_offset,uw_factor)

    def project_uv(self,index):
        from modelling_projection import project
        return project(self,index)

    def texture(self,value):
        from modelling_texture import texture
        return texture(self,value)

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
        from modelling_channels import additional_channels
        if additional_channels(self.geometry):raise NotImplementedError('Extrusion of additional vertex channels remains pending')
        from modelling_extrude import extrude
        from modelling_uv import inherit_primary
        return inherit_primary(extrude(self,distance,mode),self)

    def offset(self,distance,keep=None):
        from modelling_offset import offset
        return offset(self,distance,keep)

    def roof_shed(self,value,index=0,mode='byAngle'):
        from modelling_roof_shed import roof_shed
        return roof_shed(self,value,index,mode)

    def roof_ridge(self,value,mode='byAngle'):
        from modelling_roof_ridge import roof_ridge
        return roof_ridge(self,value,mode)

    def insert(self,asset,mode='alignSizeAndPosition',cleanup='noCleanup'):
        from modelling_insert import insert
        return insert(self,asset,mode,cleanup)

    def color(self,*values):
        from modelling_material import color
        return color(self,*values)

    def set_material(self,values):
        from modelling_material import set_material
        return set_material(self,values)

    def get_material(self,source='used',mode='changed'):
        from modelling_material import get_material
        return get_material(self,source,mode)

    def reset_material(self,target='shape'):
        from modelling_material import reset_material
        return reset_material(self,target)

    def resolved_materials(self):
        from modelling_material import resolved_materials
        return resolved_materials(self)

    def scale_uv(self,index,u,v):
        from modelling_uv import scale_uv
        return scale_uv(self,index,u,v)

    def rotate_uv(self,index,angle):
        from modelling_uv import rotate_uv
        return rotate_uv(self,index,angle)

    def translate_uv(self,index,u,v):
        from modelling_uv import translate_uv
        return translate_uv(self,index,u,v)

    def transform_uv(self,index,matrix,offset=(0.,0.)):
        from modelling_uv import transform_uv
        return transform_uv(self,index,matrix,offset)

    def copy_uv(self,source,destination):
        from modelling_uv import copy_uv
        return copy_uv(self,source,destination)

    def delete_uv(self,index):
        from modelling_uv import delete_uv
        return delete_uv(self,index)

    def normalize_uv(self,index,mode='uv',kind='collectiveAllFaces'):
        from modelling_uv_normalize import normalize_uv
        return normalize_uv(self,index,mode,kind)

    def is_instanced(self):
        return (self.asset is not None and self.geometry.unit_vertices is self.asset.geometry.unit_vertices
                and self.geometry.fields is self.asset.geometry.fields
                and self.geometry.reference_size==self.asset.geometry.reference_size)

    def comp(self,component,*branches,alignment='zUp'):
        from modelling_channels import additional_channels
        if component not in ('f','g','m') and additional_channels(self.geometry):
            raise NotImplementedError('Additional vertex channels in wire/hole components remain pending')
        from modelling_components import split_components
        from modelling_uv import inherit_primary
        return tuple(replace(inherit_primary(child,self),material=self.material,textures=self.textures,projections=self.projections) for child in split_components(self,component,branches,alignment))

    def split(self,axis,*parts,repeat=False,adjust=True,cut_material=None,cut_group=0,max_parts=10000):
        from modelling_channels import additional_channels
        if additional_channels(self.geometry):raise NotImplementedError('Splitting additional vertex channels remains pending')
        from modelling_split import split
        from modelling_uv import inherit_primary
        return tuple(inherit_primary(child,self) for child in split(self,axis,parts,repeat,adjust,cut_material,cut_group,max_parts))

    def split_area(self,axis,*parts,repeat=False,adjust=True,max_parts=10000):
        from modelling_channels import additional_channels
        if additional_channels(self.geometry):raise NotImplementedError('Area splitting additional vertex channels remains pending')
        from modelling_area_split import split_area
        from modelling_uv import inherit_primary
        return tuple(inherit_primary(child,self) for child in split_area(self,axis,parts,repeat,adjust,max_parts))

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
        prepared=(self.asset is not None and self.geometry.unit_vertices is self.asset.geometry.unit_vertices
                  and self.geometry.reference_size==self.asset.geometry.reference_size)
        size=self.asset.render_size(self.scope.size) if prepared else self.scope.size
        matrix=np.eye(4) if prepared else self.scope.matrix
        if prepared:
            matrix[:3,:3]=self.scope.axes*np.asarray(size);matrix[:3,3]=self.scope.origin
        vertices=self.geometry.unit_vertices@matrix[:3,:3].T+matrix[:3,3]
        if not np.isfinite(vertices).all():raise ValueError('Shape transform produced non-finite coordinates')
        result=dict(self.geometry.fields)|dict(vertices=vertices)
        for name in ('normals','edges_normals','points_normals'):
            if name not in result:continue
            scale=np.divide(size,self.geometry.reference_size,out=np.ones(3),
                            where=np.asarray(self.geometry.reference_size)!=0)
            if np.any(scale==0):raise ValueError('Singular authored-normal transforms need a verified cleanup contract')
            normals=result[name]@(np.diag(1/scale)@self.scope.axes.T)
            length=np.linalg.norm(normals,axis=1)
            result[name]=np.divide(normals,length[:,None],out=np.zeros_like(normals),where=length[:,None]!=0)
        return result
