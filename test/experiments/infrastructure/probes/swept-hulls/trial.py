from pathlib import Path
import json,time
import numpy as np,shapely
import hulls
import infrastructure_junction_turns as j
import infrastructure_turn_paths as t
import infrastructure_corridor_experiment as c
from infrastructure_curvature_paths import fit_curvature_paths
v=json.loads(Path('test/experiments/infrastructure/infrastructure_network_recipes.json').read_text())['vehicleProfiles']['compact']
j.turn_body_check=t.turn_body_check=hulls.turn_check
original=c.solve_module

def inspect(module,*args,**kwargs):
    result=original(module,*args,**kwargs)
    if module==5373:print('long',result[0],flush=True)
    return result
c.solve_module=inspect
for name in ('Wien','ZuerichHauptbahnhof','BaselBadischerBahnhof'):
    r,_=c.survey(Path('build/experiments/infrastructure'),name,v,96)
    print(name,r['counts'],r['total_ms'],flush=True)
