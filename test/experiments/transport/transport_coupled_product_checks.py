"""Reject lost, duplicate, invented or unexplained requests in partial road products."""

import sys
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).parent/'probes/roadbeds'))
from steering_roadbed_product_probe import declared_public_crossing
from transport_travel import MODES

from transport_coupled_roadbed_products import validate_request_ledger,shared_public_addition


def verify_request_ledger():
    a=((1,2,1),(3,4,-1));b=((3,4,1),(1,2,-1));unknown=((5,6,1),(7,8,-1))
    pairs={a:(0,1),b:(1,0)}
    proved=dict(request=a);omitted=dict(request=b,reason='no_constructive_candidate')
    validate_request_ledger(pairs,[proved],[omitted])
    validate_request_ledger(pairs,[proved,dict(request=b)],[])
    invalid=[([proved],[]),([proved,proved],[omitted]),([proved],[omitted,omitted]),
             ([proved,dict(request=b)],[omitted]),([proved],[dict(request=unknown,reason='absent')]),
             ([proved],[dict(request=b)])]
    for proofs,unresolved in invalid:
        try:
            validate_request_ledger(pairs,proofs,unresolved)
        except AssertionError:
            continue
        raise AssertionError('invalid request ledger accepted')
    return dict(partial_and_complete_products_checked=True,invalid_ledgers_rejected=len(invalid))


def verify_shared_public_area():
    physical=shapely.box(0,0,4,4)
    addition=shapely.box(1,1,3,3)
    quantum=2.**-42
    assert shared_public_addition({},physical,quantum).is_empty
    assert shared_public_addition(dict(shared_public_addition_wkb=addition.wkb.hex()),physical,quantum).equals(addition)
    for invalid in (shapely.box(3,1,5,3),shapely.LineString([[1,1],[2,2]])):
        try:shared_public_addition(dict(shared_public_addition_wkb=invalid.wkb.hex()),physical,quantum)
        except AssertionError:pass
        else:raise AssertionError('unbacked shared public area accepted')
    return dict(legacy_products_unchanged=True,declared_shared_public_area=True,unbacked_area_rejected=True)


def verify_declared_crossings():
    line=shapely.LineString([[-2,0],[2,0]]);native=shapely.box(-2,-2,2,2)
    graph=dict(lines=np.array([line],dtype=object),half_widths=np.array([2.]),
               travel_public=np.array([[MODES['walk'],MODES['walk']]]),travel_restricted=np.array([[0,0]]))
    network=SimpleNamespace(graph=graph,tree=shapely.STRtree(graph['lines']),
        mesh=(np.array([native],dtype=object),shapely.STRtree([native])),product=dict(face_owners=np.array([7])))
    crossing=shapely.box(-.5,-.5,.5,.5)
    shared,owners=declared_public_crossing(network,native,crossing)
    assert shared.equals(crossing) and owners=={7}
    empty,owners=declared_public_crossing(network,native,shapely.Polygon())
    assert empty.is_empty and not owners
    for public,restricted in ((MODES['walk'],MODES['walk']),
                              (MODES['rail'],0),(MODES['motor'],0)):
        graph['travel_public'][:]=public;graph['travel_restricted'][:]=restricted
        try:declared_public_crossing(network,native,crossing)
        except AssertionError:pass
        else:raise AssertionError('private or incompatible crossing admitted')
    return dict(public_pedestrian_crossing_owned=True,private_rail_and_motor_crossings_rejected=True)


if __name__=='__main__':
    print(verify_request_ledger()|verify_shared_public_area()|verify_declared_crossings())
