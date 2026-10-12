"""A staged rebuild must keep baseline requests and reject lost deferred requests."""

import json
from pathlib import Path
import sys

import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_coupled_roadbed_experiment as coupled
from transport_shared_road_space import construct_shared_space


def verify():
    poses=[dict(section=i,edge=i,direction=1,station=0.) for i in range(3)]
    baseline=coupled.request_keys(poses,[(0,1)])
    all_requests=baseline|coupled.request_keys(poses,[(1,2)])
    seen=[];published=[(0,1)]
    def build(*args):
        seen.append(args[-2])
        return dict(poses=poses,base=shapely.box(0,0,10,10),planned=shapely.box(0,0,10,10),
                    unresolved=[],curves=[dict(pair=p) for p in published])
    args=(None,None,0,set(),{0},None,None,None,all_requests)
    try:construct_shared_space(build,*args,baseline_required=baseline)
    except AssertionError as error:assert str(error)=='previous routes lost after the shared-space retry'
    else:raise AssertionError('unproved deferred request was published')
    assert seen==[baseline]
    published.append((1,2));seen.clear()
    product=construct_shared_space(build,*args,baseline_required=baseline)
    assert seen==[baseline] and len(product['curves'])==2
    seen.clear();construct_shared_space(build,*args)
    assert seen==[all_requests]
    return dict(baseline_retained=True,deferred_requests_required_at_publication=True,
                missing_deferred_rejected=True,default_requires_every_request_immediately=True)


if __name__=='__main__':print(json.dumps(verify()))
