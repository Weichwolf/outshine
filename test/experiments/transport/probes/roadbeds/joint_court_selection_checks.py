"""Reject individually valid courts that jointly erase their traffic island."""

import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import shapely
from joint_court_selection import select_layout


def verify():
    island=shapely.box(0,0,4,4)
    floor=shapely.box(-5,-5,9,9).difference(island)
    left=dict(id='left',cost=1,addition=shapely.box(0,0,2,4))
    right=dict(id='right',cost=1,addition=shapely.box(2,0,4,4))
    top=dict(id='top',cost=2,addition=shapely.box(0,2,4,4))
    selected,audit=select_layout(floor,[[left],[right,top]])
    assert [c['id'] for c in selected]==['left','top'] and audit['trials']==2
    selected,audit=select_layout(floor,[[left],[right,top]],1)
    assert selected is None and audit['reason']=='selection_budget_exhausted'
    selected,audit=select_layout(floor,[[left],[right]])
    assert selected is None and audit['reason']=='no_compatible_layout'
    print('joint court selection: PASS; full vehicle/mesh proof remains downstream')


if __name__=='__main__':verify()
