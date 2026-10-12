"""Close unusable split pockets while preserving each source island and reserved space."""

import numpy as np
import shapely

from infrastructure_turn_roadbeds import bounded_voids,preserves_voids,preserves_coverage


def close_split_pockets(surface,support,reserved,minimum_width=1.):
    if not np.isfinite(minimum_width) or minimum_width<=0:raise ValueError('island width must be finite and positive')
    source=bounded_voids(surface);planned=shapely.union_all([surface,support])
    pockets=bounded_voids(planned)
    if preserves_voids(surface,planned):return support,dict(closed_pockets=0,closed_area_m2=0.)
    pairs=shapely.STRtree(pockets).query(source,predicate='intersects')
    pairs=pairs[:,shapely.area(shapely.intersection(source[pairs[0]],pockets[pairs[1]]))>0]
    pairs=pairs[:,np.argsort(pairs[0],kind='stable')]
    counts=np.bincount(pairs[1],minlength=len(pockets));fills=[]
    owners,starts=np.unique(pairs[0],return_index=True)
    for owner,begin,end in zip(owners,starts,np.r_[starts[1:],len(pairs[0])]):
        ids=pairs[1,begin:end]
        if len(ids)<2 or np.any(counts[ids]!=1):continue
        fragments=pockets[ids]
        anchor=shapely.point_on_surface(source[owner])
        kept=np.flatnonzero(shapely.covers(fragments,anchor))
        if len(kept)!=1:continue
        if kept[0]!=int(np.argmax(shapely.area(fragments))):continue
        discarded=np.delete(fragments,kept[0])
        if any(p.area>minimum_width**2 or not p.buffer(-minimum_width*.5).is_empty or
               p.intersects(reserved) or not preserves_coverage(p,source[owner]) for p in discarded):continue
        fills.extend(discarded)
    if not fills:return support,dict(closed_pockets=0,closed_area_m2=0.)
    filled=shapely.union_all(fills);result=shapely.union_all([support,filled])
    if not preserves_voids(surface,shapely.union_all([surface,result])):
        return support,dict(closed_pockets=0,closed_area_m2=0.)
    return result,dict(closed_pockets=len(fills),closed_area_m2=filled.area,
                       minimum_island_width_m=minimum_width,maximum_pocket_area_m2=minimum_width**2)


def verify_split_pockets():
    outer=shapely.box(-2,-2,12,8);hole=shapely.box(0,0,10,6)
    floor=outer.difference(hole);support=shapely.box(-1,.4,11,.5)
    assert not preserves_voids(floor,floor.union(support))
    fixed,note=close_split_pockets(floor,support,shapely.Polygon())
    assert note['closed_pockets']==0,'a large thin island was filled'
    small=shapely.box(0,0,4,4);floor=outer.difference(small)
    support=shapely.box(-1,.15,5,.3)
    fixed,note=close_split_pockets(floor,support,shapely.Polygon())
    assert note['closed_pockets']==1 and abs(note['closed_area_m2']-.6)<1e-12
    assert preserves_voids(floor,floor.union(fixed))
    reserved=shapely.box(.2,0,.3,.1)
    kept,note=close_split_pockets(floor,support,reserved)
    assert kept.equals(support) and note['closed_pockets']==0
    kept,note=close_split_pockets(floor,shapely.box(-1,1,5,1.2),shapely.Polygon())
    assert note['closed_pockets']==0 and kept.equals(shapely.box(-1,1,5,1.2))
    open_island=shapely.box(0,0,4,4).difference(shapely.box(1.8,2,2.2,5))
    bridge=shapely.box(1.8,3.9,2.2,4.1)
    assert len(bounded_voids(open_island))==0 and len(bounded_voids(open_island.union(bridge)))==1
    kept,note=close_split_pockets(open_island,bridge,shapely.Polygon())
    assert kept.equals(bridge) and note['closed_pockets']==0
    return dict(unusable_split_pocket_closed=True,source_anchor_retained=True,
                reserved_and_large_fragments_retained=True,unrelated_new_voids_not_repaired=True)


if __name__=='__main__':print(verify_split_pockets())
