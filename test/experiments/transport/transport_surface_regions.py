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


def has_resolvable_interior(geometry,roundoff):
    parts=shapely.get_parts(polygonal_region(geometry))
    return bool(np.any(~shapely.is_empty(shapely.buffer(parts,-roundoff,quad_segs=16))))


def canonical_grid_region(geometry,quantum):
    if not np.isfinite(quantum) or quantum<=0:raise ValueError('surface quantum must be finite and positive')
    source=shapely.set_precision(geometry,0,mode='pointwise')
    result=shapely.normalize(polygonal_region(shapely.set_precision(source,quantum)))
    difference=source.symmetric_difference(shapely.set_precision(result,0,mode='pointwise'))
    scale=max(1.,float(np.abs(shapely.get_coordinates(source)).max(initial=0)))
    roundoff=64*np.finfo(float).eps*scale
    assert difference.area<=roundoff*source.length,'grid normalization changes significant area'
    assert not has_resolvable_interior(difference,roundoff),'grid normalization changes a real-width feature'
    return result


def verify_canonical_grid():
    quantum=2.**-42
    polygon=shapely.Polygon([[0,0],[4,0],[4,4],[2,4],[2.25,4+quantum],[2-quantum,4],[0,4]])
    declared=shapely.set_precision(polygon,quantum,mode='pointwise')
    live=canonical_grid_region(declared,quantum)
    restored=canonical_grid_region(shapely.from_wkb(declared.wkb),quantum)
    assert live.equals_exact(restored,0) and live.is_valid
    assert canonical_grid_region(live,quantum).equals_exact(live,0)
    for source in (shapely.box(0,0,.00001,4),shapely.box(0,0,4,4).difference(shapely.box(1,1,2,2))):
        result=canonical_grid_region(source,quantum)
        assert not result.is_empty and len(result.interiors)==len(source.interiors)
        assert np.max(np.abs(np.array(result.bounds)-source.bounds))<=quantum
    try:canonical_grid_region(shapely.box(0,0,.04,1),.1)
    except AssertionError:pass
    else:raise AssertionError('real-width collapse accepted')
    return dict(live_and_restored_geometry_equal=True,normalization_idempotent=True,
        narrow_faces_and_holes_preserved=True,real_width_collapse_rejected=True)


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


if __name__=='__main__':print(verify()|verify_canonical_grid())
