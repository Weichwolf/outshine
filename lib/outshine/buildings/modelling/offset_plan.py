"""Planar signed offsets and oppositely oriented exterior borders, preserving every component."""

from dataclasses import dataclass
import math

import shapely


@dataclass(frozen=True)
class Region:
    polygon: object
    kind: str
    normal_sign: int


@dataclass(frozen=True)
class OffsetPlan:
    regions: tuple
    occluder: object
    bounds: object


def offset(polygon,distance,keep=None):
    if not isinstance(polygon,shapely.Polygon) or polygon.is_empty or not polygon.is_valid:
        raise ValueError('Offset requires one valid nonempty planar polygon')
    if polygon.has_z or not all(math.isfinite(value) for value in polygon.bounds):
        raise ValueError('Planar offset requires finite XY coordinates')
    if isinstance(distance,bool) or not math.isfinite(distance):raise ValueError('Offset distance must be finite')
    if keep not in (None,'all','inside','border'):raise ValueError('Unknown offset selector')
    inside=polygon.buffer(distance,join_style='mitre',mitre_limit=float('inf')) if distance else polygon
    if not inside.is_valid:raise ValueError('Offset produced invalid geometry')
    border=polygon.difference(inside) if distance<=0 else inside.difference(polygon)
    rows=[]
    for geometry,kind,sign in ((inside,'inside',1),(border,'border',1 if distance<=0 else -1)):
        if keep not in (None,'all',kind):continue
        for part in shapely.get_parts(geometry):
            if part.is_empty:continue
            if not isinstance(part,shapely.Polygon):raise ValueError('Offset produced a nonpolygonal component')
            rows.append(Region(shapely.orient_polygons(part,exterior_cw=sign<0),kind,sign))
    bounds=shapely.union_all([row.polygon for row in rows]).bounds if rows else None
    return OffsetPlan(tuple(rows),polygon if keep is not None else None,bounds)
