"""Metrical CGA scope operations in the right-handed Esri (east, up, south) axes."""

from dataclasses import dataclass,replace
from functools import cached_property
import math
from numbers import Real

import numpy as np


def scalar(value):
    if isinstance(value,bool) or not isinstance(value,Real) or not math.isfinite(value):
        raise ValueError('Scope parameters require finite real numbers')
    return float(value)


def triple(values):
    if len(values)!=3:raise ValueError('A scope vector requires three coordinates')
    return tuple(scalar(value) for value in values)


def rotation(angles):
    x,y,z=np.deg2rad(triple(angles));cx,cy,cz=np.cos([x,y,z]);sx,sy,sz=np.sin([x,y,z])
    rx=np.array([[1.,0.,0.],[0.,cx,-sx],[0.,sx,cx]])
    ry=np.array([[cy,0.,sy],[0.,1.,0.],[-sy,0.,cy]])
    rz=np.array([[cz,-sz,0.],[sz,cz,0.],[0.,0.,1.]])
    return rz@ry@rx


def fixed(array):
    if not np.isfinite(array).all():raise ValueError('Scope transform exceeds finite coordinates')
    array.setflags(write=False)
    return array


@dataclass(frozen=True)
class Relative:
    value: float

    def __post_init__(self):object.__setattr__(self,'value',scalar(self.value))


def resolved(values,sizes):
    return tuple(value.value*size if isinstance(value,Relative) else scalar(value)
                 for value,size in zip(values,sizes,strict=True))


@dataclass(frozen=True)
class Frame:
    position: tuple=(0.,0.,0.)
    orientation: tuple=(0.,0.,0.)

    def __post_init__(self):
        object.__setattr__(self,'position',triple(self.position))
        object.__setattr__(self,'orientation',triple(self.orientation))

    @cached_property
    def axes(self):return fixed(rotation(self.orientation))


@dataclass(frozen=True)
class Scope:
    position: tuple=(0.,0.,0.)
    orientation: tuple=(0.,0.,0.)
    size: tuple=(1.,1.,1.)
    pivot: Frame=Frame()
    object_frame: Frame=Frame()

    def __post_init__(self):
        for name in ('position','orientation','size'):object.__setattr__(self,name,triple(getattr(self,name)))
        if not isinstance(self.pivot,Frame) or not isinstance(self.object_frame,Frame):
            raise ValueError('Scope pivot and object coordinates require rigid frames')

    @cached_property
    def pivot_axes(self):return fixed(self.object_frame.axes@self.pivot.axes)

    @cached_property
    def pivot_origin(self):return fixed(np.asarray(self.object_frame.position)+self.object_frame.axes@self.pivot.position)

    @cached_property
    def axes(self):return fixed(self.pivot_axes@rotation(self.orientation))

    @cached_property
    def origin(self):return fixed(self.pivot_origin+self.pivot_axes@self.position)

    @cached_property
    def centre(self):return fixed(self.origin+self.axes@(np.asarray(self.size)/2))

    @cached_property
    def matrix(self):
        result=np.eye(4);result[:3,:3]=self.axes@np.diag(self.size);result[:3,3]=self.origin
        return fixed(result)

    def t(self,x,y,z):
        delta=rotation(self.orientation)@resolved((x,y,z),self.size)
        return replace(self,position=tuple(np.asarray(self.position)+delta))

    def r(self,x,y,z,centre='scopeOrigin'):
        if centre not in ('scopeOrigin','scopeCenter'):raise ValueError('Unknown CGA rotation centre')
        angles=tuple(np.asarray(self.orientation)+triple((x,y,z)))
        position=np.asarray(self.position)
        if centre=='scopeCenter':
            half=np.asarray(self.size)/2
            position=position+rotation(self.orientation)@half-rotation(angles)@half
        return replace(self,position=tuple(position),orientation=angles)

    def s(self,x,y,z):return replace(self,size=resolved((x,y,z),self.size))

    def translate(self,mode,coordinates,x,y,z):
        if mode not in ('abs','rel'):raise ValueError('Translation mode must be abs or rel')
        if coordinates=='world':origin,axes=np.zeros(3),np.eye(3)
        elif coordinates=='object':origin,axes=np.asarray(self.object_frame.position),self.object_frame.axes
        elif coordinates=='pivot':origin,axes=self.pivot_origin,self.pivot_axes
        elif coordinates=='scope':origin,axes=self.origin,self.axes
        else:raise ValueError('Unknown CGA coordinate system')
        world=(origin if mode=='abs' else self.origin)+axes@resolved((x,y,z),self.size)
        position=self.pivot_axes.T@(world-self.pivot_origin)
        return replace(self,position=tuple(position))
