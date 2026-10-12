"""Regularize area products without changing any positive-area component."""

import numpy as np
import shapely


def polygonal_region(geometry):
    if geometry.geom_type in ('Polygon','MultiPolygon'):return geometry
    pending=shapely.get_parts(geometry);polygons=[]
    while len(pending):
        types=shapely.get_type_id(pending)
        polygons.extend(pending[types==3])
        pending=shapely.get_parts(pending[np.isin(types,[6,7])])
    result=(polygons[0] if len(polygons)==1 else shapely.MultiPolygon(polygons)) if polygons else shapely.Polygon()
    assert result.is_valid
    assert result.area==geometry.area
    return result


def verify():
    square=shapely.box(0,0,4,4).difference(shapely.box(1,1,2,2))
    remote=shapely.box(8,0,9,1)
    line=shapely.LineString([[-1,0],[1,0]])
    source=shapely.GeometryCollection([square,line,shapely.Point(7,0)])
    region=polygonal_region(source)
    assert region.equals(square) and len(region.interiors)==1
    assert polygonal_region(line).is_empty
    nested=shapely.GeometryCollection([source,shapely.GeometryCollection([remote,shapely.Point(12,0)])])
    parts=polygonal_region(nested)
    assert parts.equals(shapely.union_all([square,remote])) and len(shapely.get_parts(parts))==2
    narrow=shapely.box(20,0,20.00001,4)
    kept=polygonal_region(shapely.GeometryCollection([narrow,line]))
    assert kept.equals(narrow) and kept.area>0
    moved=shapely.transform(nested,lambda p:p+[1000,-700])
    expected=shapely.transform(parts,lambda p:p+[1000,-700])
    assert polygonal_region(moved).equals(expected)
    assert line.equals(shapely.LineString([[-1,0],[1,0]]))
    return dict(zero_area_fragments_separated=True,positive_area_and_holes_preserved=True,
        narrow_faces_preserved=True,disconnected_components_preserved=True,nested_regions=True,
        rigid_transform=True,source_lines_unchanged=True)


if __name__=='__main__':print(verify())
