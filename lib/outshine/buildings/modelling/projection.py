"""Captured metric scope/world projection, inherited independently of later component scopes."""

from dataclasses import dataclass,replace
import math

import numpy as np

from .scope import Relative,scalar
from .split_sizes import Floating
from .uv import channel,surface,changed,face_channel


@dataclass(frozen=True)
class Projection:
    channel: int
    matrix: tuple
    offset: tuple

    def __post_init__(self):
        object.__setattr__(self,'channel',channel(self.channel))
        matrix=np.asarray(self.matrix,dtype=float);offset=np.asarray(self.offset,dtype=float)
        if matrix.shape!=(2,3) or offset.shape!=(2,) or not np.isfinite(matrix).all() or not np.isfinite(offset).all():
            raise ValueError('Projection requires a finite 2x3 matrix and offset pair')
        object.__setattr__(self,'matrix',tuple(tuple(row) for row in matrix))
        object.__setattr__(self,'offset',tuple(offset))


def projections(values):
    result={}
    for value in values:
        if not isinstance(value,Projection) or value.channel in result:raise ValueError('Invalid or duplicate projection channel')
        result[value.channel]=value
    return tuple(result[key] for key in sorted(result))


def raw_dimension(value,size):
    if isinstance(value,Floating):
        requested=abs(value.value);value=scalar(size)
        if requested>1e-25:
            extent=abs(value);count=extent/requested
            count=round(count) if count<2**52 else count
            value=extent/count if count>=1 else extent
    else:value=scalar(value.value*size) if isinstance(value,Relative) else scalar(value)
    return value


def dimension(value,size):
    value=raw_dimension(value,size)
    return math.copysign(max(abs(value),1e-8),value) if value else 1e-8


def resolved_offset(value,requested,size):
    if isinstance(value,Relative):return scalar(value.value*size)
    if isinstance(value,Floating):
        if isinstance(requested,Floating) and abs(requested.value)>1e-25:
            return scalar(raw_dimension(requested,size)/requested.value*value.value)
        return value.value
    return scalar(value)


def setup(shape,index,axes,width,height,width_offset=0.,height_offset=0.,uw_factor=0.):
    index=channel(index)
    selectors={'xy':(0,1,2,1), 'xz':(0,2,1,1), 'yx':(1,0,2,-1),
               'yz':(1,2,0,1), 'zx':(2,0,1,1), 'zy':(2,1,0,-1)}
    if not isinstance(axes,str):raise ValueError('Projection axes require a named frame and axis pair')
    frame,separator,pair=axes.partition('.')
    if separator!='.' or frame not in ('scope','world') or pair not in selectors:
        raise ValueError('Unknown projection frame or axis pair')
    u,v,w,depth_sign=selectors[pair]
    offsets=[resolved_offset(width_offset,width,shape.scope.size[u]),resolved_offset(height_offset,height,shape.scope.size[v])]
    width=dimension(width,shape.scope.size[u]);height=dimension(height,shape.scope.size[v])
    offsets=np.array([math.fmod(offsets[0],width*1000),math.fmod(offsets[1],height*1000)])
    local=np.zeros((2,3));local[0,u]=1/width;local[1,v]=1/height
    local[0,w]=depth_sign*scalar(uw_factor)/width
    matrix=local@shape.scope.axes.T if frame=='scope' else local
    offset=-offsets/[width,height]
    if frame=='scope':offset=offset-matrix@shape.scope.origin
    value=Projection(index,tuple(map(tuple,matrix)),tuple(offset))
    return replace(shape,projections=tuple(row for row in shape.projections if row.channel!=index)+(value,))


def project(shape,index):
    index=channel(index);surface(shape)
    values={row.channel:row for row in shape.projections}
    if index not in values:raise ValueError('UV channel has no explicit projection setup')
    projection=values[index]
    vertices=shape.emit()['vertices']
    stream=vertices@np.asarray(projection.matrix).T+projection.offset
    faces=[face_channel(face,index,tuple(tuple(tuple(pair) for pair in stream[list(ring)])
        for ring in (face.outer,*face.holes))) for face in shape.geometry.polygons]
    return changed(shape,index,stream,faces)
