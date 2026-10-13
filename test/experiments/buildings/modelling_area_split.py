"""Area-targeted 3D surface cuts using exact triangle area fractions along a scope axis."""

import numpy as np

from modelling_cut_source import prepare_source
from modelling_split import emit_parts
from modelling_split_area import TriangleArea
from modelling_split_sizes import Span,allocations


COS30=np.sqrt(3.)/2.


def area_profile(source,dimension):
    if source.closed:raise ValueError('Area subdivision applies to open surfaces, not closed solids')
    points=source.properties[source.roots[source.triangles],:3]
    area=np.linalg.norm(np.cross(points[:,1]-points[:,0],points[:,2]-points[:,0]),axis=1)*.5
    faces=np.zeros((len(source.polygons),3))
    np.add.at(faces,source.triangle_parts,source.geometric_normals*area[:,None])
    lengths=np.linalg.norm(faces,axis=1)
    if np.any(lengths<=1e-12):raise ValueError('Area subdivision face has no consistent normal')
    faces/=lengths[:,None]
    normal=faces.sum(axis=0);length=np.linalg.norm(normal)
    if length<=1e-12:raise ValueError('Area subdivision has no consistent surface plane')
    normal/=length
    if np.any(abs(source.geometric_normals@normal)<COS30-1e-12):raise ValueError('Area subdivision surface is too nonplanar')
    axis=source.shape.scope.axes[:,dimension]
    if axis@normal>COS30+1e-12:raise ValueError('Area subdivision axis is too close to the surface normal')
    direction=axis*(1. if source.shape.scope.size[dimension]>=0 else -1.)
    coordinates=(points-source.shape.scope.origin)@direction
    return TriangleArea.from_triangles(coordinates,area)


def split_area(shape,axis,parts,repeat=False,adjust=True,max_parts=10000):
    if axis not in ('x','y','z'):raise ValueError('Area split axis must be x, y or z')
    if not isinstance(adjust,bool):raise ValueError('Scope adjustment must be boolean')
    allocations(0.,parts,repeat,max_parts)
    if not parts:return ()
    dimension=('x','y','z').index(axis);source=prepare_source(shape);cumulative=area_profile(source,dimension)
    areas=allocations(cumulative.total,parts,repeat,max_parts)
    if not areas:return ()
    low=float(cumulative.values.min());high=float(cumulative.values.max())
    scope_low=min(0.,low);scope_high=max(abs(shape.scope.size[dimension]),high)
    targets=sorted({span.start for span in areas}|{span.stop for span in areas})
    boundaries={target:scope_low if target==0 else scope_high if target==cumulative.total else
                cumulative.inverse(target,low,high) for target in targets}
    spans=tuple(Span(boundaries[span.start],boundaries[span.stop],span.label,span.pattern_index,span.index,span.total) for span in areas)
    return emit_parts(shape,source,axis,dimension,spans,adjust,area_spans=areas)
