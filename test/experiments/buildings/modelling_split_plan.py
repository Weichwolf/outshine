"""Metric planar split prototype; native mesh, UV cuts and nested syntax remain separate work."""

from dataclasses import dataclass

import shapely

from modelling_split_area import TriangleArea,inverse_area
from modelling_split_sizes import allocations


@dataclass(frozen=True)
class PlanPart:
    geometry: object
    scope_bounds: tuple
    span: object
    cuts: tuple


def clipped(polygon,axis,low,high):
    bounds=list(polygon.bounds);bounds[axis]=low;bounds[axis+2]=high
    result=polygon.intersection(shapely.box(*bounds))
    if result.geom_type in ('Polygon','MultiPolygon'):return result
    parts=[part for part in shapely.get_parts(result) if part.geom_type in ('Polygon','MultiPolygon') and part.area>0]
    return shapely.union_all(parts) if parts else shapely.Polygon()


def split_plan(polygon,axis,parts,repeat=False,area=False,adjust=True,method='triangles',max_parts=10000):
    if polygon.geom_type not in ('Polygon','MultiPolygon') or polygon.is_empty or not polygon.is_valid:
        raise ValueError('Split needs valid nonempty planar polygon geometry')
    if axis not in ('x','y'):raise ValueError('A plan split axis must be x or y in its supplied 2D coordinates')
    if not isinstance(area,bool) or not isinstance(adjust,bool):raise ValueError('Area and scope adjustment must be boolean')
    if method not in ('triangles','triangles-bisect','geos'):raise ValueError('Unknown cumulative-area method')
    axis=('x','y').index(axis);bounds=polygon.bounds;low,high=bounds[axis],bounds[axis+2]
    spans=allocations(polygon.area if area else high-low,parts,repeat,max_parts)
    if area:
        cumulative=TriangleArea(polygon,axis) if method.startswith('triangles') else lambda t:clipped(polygon,axis,low,t).area
        inverse=cumulative.inverse if method=='triangles' else lambda target,begin,end:inverse_area(cumulative,target,begin,end)
        targets=sorted({span.start for span in spans}|{span.stop for span in spans})
        boundaries={target:low if target==0 else high if target==polygon.area else
                    inverse(target,low,high) for target in targets}
    else:boundaries={target:low+target for span in spans for target in (span.start,span.stop)}
    result=[]
    for span in spans:
        begin,end=boundaries[span.start],boundaries[span.stop];geometry=clipped(polygon,axis,begin,end)
        scope=list(bounds);scope[axis]=begin;scope[axis+2]=end
        result.append(PlanPart(geometry,geometry.bounds if adjust and not geometry.is_empty else tuple(scope),span,(begin,end)))
    return tuple(result)
