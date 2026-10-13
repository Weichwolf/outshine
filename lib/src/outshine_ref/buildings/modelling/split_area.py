"""Cumulative area from constrained triangles; inverse area without repeated geometry cuts."""

import numpy as np
import shapely


class TriangleArea:
    @classmethod
    def from_triangles(cls,coordinates,areas):
        result=cls.__new__(cls);values=np.asarray(coordinates,dtype=np.float64);weights=np.asarray(areas,dtype=np.float64)
        if values.ndim!=2 or values.shape[1]!=3 or weights.shape!=(len(values),) or not len(values):
            raise ValueError('Cumulative area needs one axis coordinate and area per triangle')
        if not np.isfinite(values).all() or not np.isfinite(weights).all() or np.any(weights<=0):
            raise ValueError('Cumulative area requires finite coordinates and positive triangle areas')
        result.values=np.sort(values,axis=1);result.areas=weights.copy();result.total=float(weights.sum())
        if np.any(result.values[:,2]==result.values[:,0]):raise ValueError('A surface triangle cannot be divided along this axis')
        return result

    def __init__(self,polygon,axis):
        triangles=list(shapely.get_parts(shapely.constrained_delaunay_triangles(polygon)))
        coordinates=np.asarray([np.asarray(triangle.exterior.coords)[:3,axis] for triangle in triangles])
        self.values=np.sort(coordinates,axis=1)
        self.areas=np.asarray([triangle.area for triangle in triangles])
        self.total=float(self.areas.sum())
        if not np.isclose(self.total,polygon.area,rtol=1e-11,atol=1e-10):
            raise ValueError('Area triangulation does not cover the source polygon')

    def prepare_inverse(self):
        self.knots=np.unique(self.values)
        self.cumulative=np.asarray([self(position) for position in self.knots])
        if np.any(np.diff(self.cumulative)<-max(1e-10,self.total*1e-12)):
            raise ValueError('Triangle cumulative area is not monotone')
        self.cumulative=np.maximum.accumulate(self.cumulative)

    def inverse(self,target,low,high):
        if not hasattr(self,'knots'):self.prepare_inverse()
        if target<=0:return low
        if target>=self.total:return high
        end=min(len(self.knots)-1,max(1,int(np.searchsorted(self.cumulative,target,side='left'))))
        begin=end-1;a,b=self.knots[begin:end+1];f0,f1=self.cumulative[begin:end+1]
        if f1==f0:return float(a)
        middle=self(a+(b-a)*.5)
        quadratic=2*(f1-2*middle+f0);linear=4*middle-f1-3*f0;delta=target-f0
        discriminant=max(0.,linear*linear+4*quadratic*delta)
        denominator=linear+np.sqrt(discriminant)
        if denominator<=0:return inverse_area(self,target,float(a),float(b))
        fraction=np.clip(2*delta/denominator,0.,1.)
        return float(a+(b-a)*fraction)

    def __call__(self,position):
        a,b,c=self.values.T;fraction=np.zeros(len(a));fraction[position>=c]=1.
        left=(position>a)&(position<b)
        fraction[left]=(position-a[left])**2/((b[left]-a[left])*(c[left]-a[left]))
        right=(position>=b)&(position<c)
        fraction[right]=1.-(c[right]-position)**2/((c[right]-a[right])*(c[right]-b[right]))
        return float(fraction@self.areas)


def inverse_area(cumulative,target,low,high):
    if target<=0:return low
    if target>=cumulative(high):return high
    # The area is monotone; a flat interval belongs to its earliest boundary.
    for _ in range(64):
        middle=low+(high-low)*.5
        if middle==low or middle==high:break
        if cumulative(middle)<target:low=middle
        else:high=middle
    return low+(high-low)*.5
