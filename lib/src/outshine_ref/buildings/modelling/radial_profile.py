"""Complete local ENU radial plans and bounded piecewise-linear profile reduction."""

from dataclasses import dataclass
import math
from numbers import Integral

import numpy as np

from .scope import scalar


@dataclass(frozen=True)
class RadialProfile:
    identifier: str
    centre_xy: tuple
    knots: tuple
    materials: tuple = (1,1,9)

    def __post_init__(self):
        if not isinstance(self.identifier,str) or not self.identifier.strip():
            raise ValueError('Radial profile needs a stable explicit identifier')
        centre=tuple(scalar(v) for v in self.centre_xy)
        if len(centre)!=2:
            raise ValueError('Radial profile needs a local XY centre')
        knots=tuple(tuple(scalar(v) for v in row) for row in self.knots)
        if len(knots)<2 or any(len(row)!=3 for row in knots):
            raise ValueError('Profile needs at least two (height, outer radius, inner radius) knots')
        values=np.asarray(knots)
        if np.any(np.diff(values[:,0])<=0) or np.any(values[:,1:]<0) or not np.any(values[:,1]>0):
            raise ValueError('Profile needs increasing heights and nonnegative nonempty radii')
        hollow=np.any(values[:,2]>0)
        if hollow and (np.any(values[:,2]<=0) or np.any(values[:,2]>=values[:,1])):
            raise ValueError('Hollow profiles need positive separated inner and outer radii throughout')
        if not hollow and np.any(values[1:-1,1]<=0):
            raise ValueError('Solid tips may occur only at profile ends')
        material=tuple(self.materials)
        if len(material)!=3 or any(isinstance(v,bool) or not isinstance(v,Integral) or not 0<=v<=65535 for v in material):
            raise ValueError('Profile needs three uint16 outer/inner/end material identifiers')
        object.__setattr__(self,'centre_xy',centre);object.__setattr__(self,'knots',knots)
        object.__setattr__(self,'materials',tuple(int(v) for v in material))

    @property
    def hollow(self):
        return self.knots[0][2]>0

    @property
    def circular_volume_m3(self):
        values=np.asarray(self.knots);r0=values[:-1,1:];r1=values[1:,1:]
        terms=np.diff(values[:,0])[:,None]*(r0*r0+r0*r1+r1*r1)/3
        return float(math.pi*np.sum(terms[:,0]-terms[:,1]))


def reduce_profile(profile,error_m):
    error=scalar(error_m)
    if error<0:
        raise ValueError('Profile error must be nonnegative metres')
    values=np.asarray(profile.knots);selected={0,len(values)-1}
    if values[0,1]==0 and values[-1,1]==0:
        selected.add(int(np.argmax(values[:,1])))
    anchors=sorted(selected);pending=list(zip(anchors[:-1],anchors[1:],strict=True))
    while pending:
        first,last=pending.pop()
        if last-first<2:
            continue
        fractions=(values[first+1:last,0]-values[first,0])/(values[last,0]-values[first,0])
        interpolated=values[first,1:]+fractions[:,None]*(values[last,1:]-values[first,1:])
        errors=np.max(abs(values[first+1:last,1:]-interpolated),axis=1)
        offset=int(np.argmax(errors))
        if errors[offset]>error:
            middle=first+1+offset;selected.add(middle);pending.extend([(first,middle),(middle,last)])
    indices=tuple(sorted(selected));return indices


def radial_error(profile,indices,sides):
    values=np.asarray(profile.knots);selected=values[list(indices)]
    errors=np.column_stack([np.interp(values[:,0],selected[:,0],selected[:,j])-values[:,j] for j in (1,2)])
    profile_error=float(np.max(abs(errors)))
    angular_error=float(np.max(values[:,1]))*(1-math.cos(math.pi/sides))
    return profile_error+angular_error,profile_error,angular_error


def detail_profile(profile,error_m,max_sides=256):
    error=scalar(error_m)
    if error<=0 or isinstance(max_sides,bool) or not isinstance(max_sides,Integral) or max_sides<3:
        raise ValueError('Detail needs positive metre error and at least three permitted sides')
    indices=reduce_profile(profile,error/2);radius=max(row[1] for row in profile.knots)
    angle=math.acos(max(-1.,1-error/(2*radius)))
    required=max(3,math.ceil(math.pi/angle)) if angle else max_sides+1
    if required>max_sides:
        raise ValueError('Angular detail budget cannot meet requested radial error')
    while radial_error(profile,indices,required)[0]>error:
        required+=1
        if required>max_sides:raise ValueError('Angular detail budget cannot meet requested radial error')
    return indices,required
