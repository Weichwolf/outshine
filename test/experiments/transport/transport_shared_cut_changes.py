"""Distinguish added road area at shared cuts from coordinate-roundoff fragments."""

import shapely

from transport_turn_roadbeds import coordinate_roundoff


def changed_shared_cut(floor,addition,sections):
    beyond_roundoff=addition.difference(floor.buffer(coordinate_roundoff(floor)))
    return bool(len(sections.query(beyond_roundoff,predicate='dwithin',distance=1e-7)))


def verify_cut_roundoff():
    for offset in (0.,1000.,1e6):
        floor=shapely.box(offset,offset,offset+10,offset+4)
        cut=shapely.LineString([[offset,offset],[offset,offset+4]])
        index=shapely.STRtree([cut]);epsilon=coordinate_roundoff(floor)
        sliver=shapely.box(offset,offset-epsilon*.25,offset+2,offset)
        assert not changed_shared_cut(floor,sliver,index)
        real=shapely.box(offset,offset-1e-4,offset+2,offset)
        assert changed_shared_cut(floor,real,index)
        assert not changed_shared_cut(floor,shapely.box(offset+5,offset-1,offset+6,offset),index)
    return dict(roundoff_sliver_ignored=True,real_cut_change_rejected=True,unrelated_addition_allowed=True)


if __name__=='__main__':print(verify_cut_roundoff())
