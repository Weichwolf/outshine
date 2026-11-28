"""Compact metrical facade axes and analytic rectangular opening coverage."""

from dataclasses import dataclass
import math

import numpy as np


def interval_area(low,high,fraction,count=None):
    """Integral of centred unit-period intervals, including negative coordinates."""
    def primitive(position):
        value=np.asarray(position,dtype=np.float64)
        if count is not None:value=np.clip(value,0.,count)
        whole=np.floor(value)
        return whole*fraction+np.clip(value-whole-(1-fraction)/2,0.,fraction)
    return primitive(high)-primitive(low)


def rectangle_coverage(centres,footprints,fractions,counts=None):
    """Exact area mean for an axis-aligned box in unit-cell coordinates.

    With counts, the repeat domain is [0,columns]×[0,rows], and area outside
    contributes zero. Without counts the pattern is periodic over the plane.
    A box is an explicit filter choice, not an EWA or shaded image-error bound.
    """
    centres=np.asarray(centres,dtype=np.float64);footprints=np.asarray(footprints,dtype=np.float64)
    fractions=np.asarray(fractions,dtype=np.float64)
    if centres.ndim<1 or centres.shape[-1]!=2 or footprints.ndim<1 or footprints.shape[-1]!=2:
        raise ValueError('Facade boxes require two-dimensional centres and footprints')
    if not np.isfinite(centres).all() or not np.isfinite(footprints).all() or np.any(footprints<=0):
        raise ValueError('Facade boxes require finite centres and positive finite footprints')
    if fractions.shape!=(2,) or not np.isfinite(fractions).all() or np.any((fractions<0)|(fractions>1)):
        raise ValueError('Opening fractions must lie in the unit interval')
    if counts is not None:
        counts=np.asarray(counts)
        if counts.shape!=(2,) or counts.dtype.kind not in 'iu' or np.any(counts<1):
            raise ValueError('Facade domain requires two positive integer counts')
    centres,footprints=np.broadcast_arrays(centres,footprints);low=centres-footprints/2;high=centres+footprints/2
    result=np.ones(centres.shape[:-1],dtype=np.float64)
    for axis in range(2):
        count=None if counts is None else counts[axis]
        value=interval_area(low[...,axis],high[...,axis],fractions[axis],count)/footprints[...,axis]
        cell=np.floor(centres[...,axis]);begin=cell+(1-fractions[axis])/2;end=cell+(1+fractions[axis])/2
        contained=(low[...,axis]>=begin)&(high[...,axis]<=end)
        if count is not None:contained&=(cell>=0)&(cell<count)
        result*=np.where(contained,1.,value)
    return np.clip(result,0.,1.)


@dataclass(frozen=True,slots=True)
class FacadeRepeat:
    width_m: float
    height_m: float
    columns: int
    rows: int
    opening_width: float
    opening_height: float

    def __post_init__(self):
        if not all(math.isfinite(value) and value>0 for value in (self.width_m,self.height_m)):
            raise ValueError('Facade dimensions must be positive finite metres')
        if any(type(value)is not int or not 1<=value<=2**32-1 for value in (self.columns,self.rows)):
            raise ValueError('Facade repeats require positive uint32 counts')
        if not all(math.isfinite(value) and 0<=value<=1 for value in (self.opening_width,self.opening_height)):
            raise ValueError('Opening fractions must lie in the unit interval')

    @property
    def cell_size_m(self):return self.width_m/self.columns,self.height_m/self.rows

    @property
    def opening_count(self):return self.columns*self.rows if self.opening_width*self.opening_height else 0

    def coverage(self,centres_m,footprints_m):
        scale=np.asarray(self.cell_size_m)
        return rectangle_coverage(np.asarray(centres_m)/scale,np.asarray(footprints_m)/scale,
            [self.opening_width,self.opening_height],[self.columns,self.rows])
