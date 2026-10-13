"""Prepare immutable geometry once and place it using documented CGA insertion modes."""

from dataclasses import dataclass,replace

import numpy as np

from modelling_scope import triple
from modelling_shape import Geometry,Shape


SCOPE_ZERO=1e-4
RATIO_EPSILON=1e-8
GEOMETRY_EPSILON=1e-25
MIN_RENDER_SIZE=1e-5
MODES=('alignSizeAndPosition','keepSizeAndPosition','keepSizeAlignPosition')


@dataclass(frozen=True)
class Asset:
    geometry: Geometry
    minimum: tuple
    name: str=''
    up_axis: str='yUp'
    import_info: object=None
    resources: object=None
    textures: tuple=()

    def __post_init__(self):
        from modelling_texture import resources
        object.__setattr__(self,'textures',resources(self.textures))
        if not isinstance(self.geometry,Geometry) or any(size<0 for size in self.geometry.reference_size):
            raise ValueError('Prepared asset requires normalized geometry with nonnegative dimensions')
        if not len(self.geometry.fields['faces']):raise ValueError('Inserted assets require surface geometry')
        if self.up_axis not in ('yUp','zUp') or not isinstance(self.name,str):raise ValueError('Invalid asset axis or name')
        object.__setattr__(self,'minimum',triple(self.minimum))

    @classmethod
    def from_shape(cls,shape,name='',up_axis='yUp'):
        if not isinstance(shape,Shape):raise ValueError('Asset preparation needs a Shape')
        if up_axis not in ('yUp','zUp'):raise ValueError('Asset up axis must be yUp or zUp')
        mesh=shape.emit()
        if not len(mesh['faces']):raise ValueError('Inserted assets require surface geometry')
        if up_axis=='zUp':
            matrix=np.array([[1.,0.,0.],[0.,0.,1.],[0.,-1.,0.]])
            mesh['vertices']=mesh['vertices']@matrix.T
            for key in ('normals','edges_normals','points_normals'):
                if key in mesh:mesh[key]=mesh[key]@matrix.T
        definitions=shape.resolved_materials() if shape.material.attributes else shape.geometry.material_defs
        geometry,scope=Geometry.from_mesh(mesh,polygons=shape.geometry.polygons,edges=shape.geometry.edges,
            points=shape.geometry.points,group_names=shape.geometry.group_names,material_defs=definitions)
        return cls(geometry,scope.position,name,up_axis,
                   shape.asset.import_info if shape.asset else None,shape.asset.resources if shape.asset else None,shape.textures)

    def render_size(self,size):
        values=np.asarray(size,dtype=float);bounds=np.asarray(self.geometry.reference_size)
        signs=np.where(values<0,-1.,1.)
        return np.where(bounds>GEOMETRY_EPSILON,signs*np.maximum(abs(values),MIN_RENDER_SIZE),1.)

    @classmethod
    def from_file(cls,path,name='',up_axis='yUp'):
        from modelling_asset_load import load_asset
        return load_asset(path,name,up_axis)


def adjusted(scope,asset):
    sizes=np.asarray(scope.size);bounds=np.asarray(asset.geometry.reference_size)
    missing=abs(sizes)<SCOPE_ZERO;count=int(missing.sum());delta=np.zeros(3)
    if count==0:return scope
    if count==3:
        sizes=bounds.copy();delta=np.asarray(asset.minimum)
    elif count:
        ratios=np.divide(sizes,bounds,out=np.ones(3),where=abs(bounds)>RATIO_EPSILON)
        factor=float(ratios[~missing].mean());sizes=sizes.copy();sizes[missing]=bounds[missing]*factor
        if count==2:delta[missing]=np.asarray(asset.minimum)[missing]*factor
    positioned=scope.t(*delta) if np.any(delta) else scope
    return replace(positioned,size=tuple(sizes))


def insert(shape,asset,mode='alignSizeAndPosition',cleanup='noCleanup'):
    if not isinstance(asset,Asset):raise ValueError('Insert needs a prepared Asset')
    if mode not in MODES:raise ValueError('Unknown insertion mode')
    if cleanup!='noCleanup':raise NotImplementedError('Numerical geometry cleanup remains a separate capability')
    scope=shape.scope
    if mode=='keepSizeAndPosition':scope=replace(scope,size=(0.,0.,0.))
    elif mode=='keepSizeAlignPosition':
        dims=np.asarray(asset.geometry.reference_size);delta=(np.asarray(scope.size)-dims)/2.;delta[1]=0.
        scope=replace(scope.t(*delta),size=tuple(dims))
    scope=adjusted(scope,asset)
    from modelling_texture import resources
    return Shape(asset.geometry,scope,shape.comp_info,shape.sweep_info,shape.split_info,asset,shape.material,resources(asset.textures,shape.textures))
