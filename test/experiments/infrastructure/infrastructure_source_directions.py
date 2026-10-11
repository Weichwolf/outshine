#!/usr/bin/env python3
"""Source direction survives duplicate handling and flat graph noding."""

import json
from pathlib import Path

import numpy as np
import shapely

from infrastructure_flat_plan import plan


def verify():
    def road(xy,**tags):
        return dict(line=shapely.LineString(xy),width=4.,properties=dict({'class':'minor'},**tags))
    recipes = json.loads(Path(__file__).with_name('infrastructure_network_recipes.json').read_text())
    forward = road([(-10,0),(10,0)],oneway=1)
    backward = road([(10,0),(-10,0)],oneway=1)
    cross = road([(0,-10),(0,10)])
    roads,graphs,usage = plan([forward,backward,forward,cross],recipes,{})
    assert usage['states']=={'used':3,'duplicate_represented':1},usage
    graph = graphs[('land',0,'ground')]
    for edge in range(len(graph['lines'])):
        first,last = graph['owner_offsets'][edge:edge+2]
        owners = graph['owner_sources'][first:last]
        directions = graph['owner_directions'][first:last]
        assert np.all(directions!=0)
        indices = [i for i,owner in enumerate(owners) if roads[owner]['properties'].get('oneway')==1]
        if indices:
            assert len(indices)==2 and directions[indices[0]]==-directions[indices[1]]
    loop = road([(0,0),(10,0),(10,10),(0,10),(0,0)],oneway=1)
    reverse = road(list(loop['line'].coords)[::-1],oneway=1)
    _,loops,ledger = plan([loop,reverse],recipes,{})
    product = loops[('land',0,'ground')]
    assert ledger['states']=={'used':2}
    for first,last in zip(product['owner_offsets'][:-1],product['owner_offsets'][1:]):
        assert sorted(product['owner_directions'][first:last].tolist())==[-1,1]
    return dict(duplicate_case=usage['states'],split_edges=len(graph['lines']),
                closed_sources=ledger['states'],unresolved_owner_directions=0)


if __name__=='__main__':
    print(json.dumps(verify()))
