"""Exact planar region supports before choosing Boolean tags or material priority."""

from dataclasses import dataclass

import numpy as np
import shapely

from outshine_ref.buildings.modelling.uv_exterior import polygon_parts


@dataclass(frozen=True)
class OverlayRegion:
    polygon: object
    supports: tuple


def regions(product,nodes,number,sources,max_cells):
    corners,normal,_,axes=nodes.projection(number)
    owner=int(product.operands[number]);native=int(product.source_triangles[number])
    original=shapely.set_precision(shapely.Polygon(corners[:,axes]),nodes.precision);other=sources[1-owner]
    if original.is_empty:raise ValueError('Original semantic triangle cannot resolve precision grid')
    coplanar=np.all(abs((other-corners[0])@normal)<=nodes.precision*4,axis=1)
    current=[OverlayRegion(original,((owner,native),))]
    if not coplanar.any():return tuple(current)
    candidates=shapely.set_precision(shapely.polygons(other[coplanar][:,:,axes]),nodes.precision);ids=np.flatnonzero(coplanar)
    pairs=shapely.STRtree(candidates).query(original,predicate='intersects')
    for candidate in pairs:
        polygon=candidates[candidate];source=int(ids[candidate]);result=[]
        for region in current:
            if len(region.supports)>1:result.append(region);continue
            inside=region.polygon.intersection(polygon)
            if inside.area<=0:result.append(region);continue
            support=tuple(sorted((*region.supports,(1-owner,source))))
            result.extend(OverlayRegion(part,support) for part in polygon_parts(inside,max_cells))
            outside=region.polygon.difference(polygon)
            result.extend(OverlayRegion(part,region.supports) for part in polygon_parts(outside,max_cells))
        current=result
        if len(current)>max_cells:raise ValueError('Semantic region capacity exceeded')
    union=shapely.union_all([r.polygon for r in current])
    tolerance=8*nodes.precision*original.length
    if union.symmetric_difference(original).area>tolerance or sum(r.polygon.area for r in current)-union.area>tolerance:
        raise ValueError('Semantic regions fail independent original-triangle coverage')
    return tuple(current)
