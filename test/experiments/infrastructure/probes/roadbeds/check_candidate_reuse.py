"""Compare scoped reuse with fresh curve evaluation after relevant input changes."""

import json
from pathlib import Path
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from infrastructure_coupled_roadbed_experiment import roadbed_candidate

vehicle=json.loads((Path(__file__).resolve().parents[2]/'infrastructure_network_recipes.json').read_text())['vehicleProfiles']['compact']
entry=dict(position=np.array([0.,0.]),heading=np.array([1.,0.]))
exit=dict(position=np.array([10.,0.]),heading=np.array([1.,0.]))
floor=shapely.box(-10,-5,20,5)
route=shapely.LineString([entry['position'],exit['position']])
base=[entry,exit,route,floor,shapely.Polygon(),vehicle,.025,False]
variants=[base]
for index,value in [(0,dict(entry,position=np.array([1.,0.]))),(1,dict(exit,heading=np.array([.99,.1]))),
                    (2,shapely.LineString([[0,0],[5,1],[10,0]])),(3,shapely.box(-10,-.2,20,.2)),
                    (3,shapely.set_precision(floor,.125)),(4,shapely.box(0,10,10,12)),
                    (5,dict(vehicle,widthM=vehicle['widthM']+1)),(6,.0125),(7,True)]:
    values=list(base);values[index]=value;variants.append(values)
cache=dict(contexts={},hits=0,misses=0)
for values in variants:
    cached=roadbed_candidate(*values,cache)
    fresh=roadbed_candidate(*values,dict(contexts={},hits=0,misses=0))
    assert cached[1:]==fresh[1:]
    assert (cached[0] is None)==(fresh[0] is None)
    if cached[0] is not None:
        assert cached[0]['path'].wkb==fresh[0]['path'].wkb
        assert cached[0]['roadbed'].wkb==fresh[0]['roadbed'].wkb
    assert roadbed_candidate(*values,cache) is cached
assert cache['misses']==len(variants) and cache['hits']==len(variants)
print(dict(input_variants=len(variants),fresh_and_reused_results_equal=True))
