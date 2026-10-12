"""Remove unusable generated roadbed voids touched by a proposed construction."""

import numpy as np
import shapely

from transport_turn_roadbeds import bounded_voids


def close_unusable_voids(surface,support,reserved,minimum_width=1.):
    if not np.isfinite(minimum_width) or minimum_width<=0:
        raise ValueError('minimum island width must be finite and positive')
    holes=bounded_voids(surface)
    candidates=shapely.STRtree(holes).query(support,predicate='intersects')
    fills=[holes[i] for i in candidates if holes[i].area<=minimum_width**2 and
           holes[i].buffer(-minimum_width*.5).is_empty and not holes[i].intersects(reserved)]
    result=shapely.union_all([surface,*fills]) if fills else surface
    return result,dict(closed_voids=len(fills),closed_area_m2=sum(h.area for h in fills),
        minimum_width_m=minimum_width,maximum_area_m2=minimum_width**2,
        filled_voids_wkb=[h.wkb.hex() for h in fills])


def verify():
    hole=shapely.box(0,0,.8,.8);outer=shapely.box(-2,-2,5,5)
    surface=outer.difference(hole);support=shapely.box(.2,-1,.4,1)
    fixed,note=close_unusable_voids(surface,support,shapely.Polygon())
    assert fixed.equals(outer) and note['closed_voids']==1
    for reserved in (hole,shapely.box(.8,0,1,.1)):
        kept,note=close_unusable_voids(surface,support,reserved)
        assert kept.equals(surface) and not note['closed_voids']
    for gap in (shapely.box(0,0,2,2),shapely.box(0,0,4,.3)):
        floor=outer.difference(gap);kept,note=close_unusable_voids(floor,support,shapely.Polygon())
        assert kept.equals(floor) and not note['closed_voids']
    kept,note=close_unusable_voids(surface,shapely.box(2,2,3,3),shapely.Polygon())
    assert kept.equals(surface) and not note['closed_voids']
    opened=surface.difference(shapely.box(.2,-3,.4,.4))
    kept,note=close_unusable_voids(opened,support,shapely.Polygon())
    assert kept.equals(opened) and not note['closed_voids']
    return dict(local_generated_void_closed=True,reserved_contacts_preserved=True,
        large_and_unrelated_voids_preserved=True,open_passage_preserved=True)


if __name__=='__main__':print(verify())
