"""Captured scope-XY projection, inherited independently of later component scopes."""

from dataclasses import dataclass,replace
import math

import numpy as np

from modelling_scope import Relative,scalar
from modelling_split_sizes import Floating
from modelling_uv import channel,surface,changed,face_channel


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


def dimension(value,size):
    if isinstance(value,Floating):
        requested=value.value;value=scalar(size)
        if requested>1e-25:
            extent=abs(value);count=extent/requested
            count=round(count) if count<2**52 else count
            value=extent/count if count>=1 else extent
    else:value=scalar(value.value*size) if isinstance(value,Relative) else scalar(value)
    return math.copysign(max(abs(value),1e-8),value) if value else 1e-8


def setup(shape,index,axes,width,height,width_offset=0.,height_offset=0.,uw_factor=0.):
    index=channel(index)
    if axes!='scope.xy':raise NotImplementedError('Projection selector signs and world frames remain under SDK verification')
    width=dimension(width,shape.scope.size[0]);height=dimension(height,shape.scope.size[1])
    offsets=np.array([math.fmod(scalar(width_offset),width*1000),math.fmod(scalar(height_offset),height*1000)])
    local=np.array([[1/width,0.,scalar(uw_factor)/width],[0.,1/height,0.]])
    matrix=local@shape.scope.axes.T
    offset=-matrix@shape.scope.origin-offsets/[width,height]
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
