"""Inspect source and return fitting on previously proved window routes."""

import argparse
from functools import partial
import json
from pathlib import Path
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from transport_cut_setback_experiment import prepared_network
from transport_cut_setbacks import restore_cut_plan
from transport_directed_routes import directed_module_routes
from transport_junction_surfaces import motor_surface
from transport_source_roadbeds import construct_route_roadbed,construct_fitted_roadbed
import transport_source_roadbeds as roadbeds
from transport_turn_roadbed_experiment import building_reservations
from transport_travel import MODES

root=Path('build/experiments/transport')
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--place',default='Wien')
parser.add_argument('--cases',nargs='+',default=['659:19:18','1066:6:4'])
parser.add_argument('--wide-first',action='store_true')
args=parser.parse_args();name=args.place
if args.wide_first:roadbeds.construct_fitted_roadbed=partial(construct_fitted_roadbed,narrow_first=False)
before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
restore_cut_plan(network,json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text())['plan'])
bounds=shapely.MultiPoint(network.product['vertices']).envelope.buffer(10)
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
buildings,_=building_reservations(receipt,bounds)
nonmotor=~np.any(network.graph['travel_public']&MODES['motor']!=0,axis=1)
other=shapely.union_all(shapely.buffer(network.graph['lines'][nonmotor],network.graph['half_widths'][nonmotor],quad_segs=2))
rows=[]
for case in args.cases:
    module,*pair=map(int,case.split(':'))
    assert len(pair)==2
    pair=tuple(pair)
    shape=network.shape(module);poses,_=network.poses(module,{})
    floor=motor_surface(shape,network.graph,network.tree,network.mesh,network.vehicle,
                        network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
    routes,_=directed_module_routes(network.graph,poses,shape,network.tree)
    entry,exit=(poses[i] for i in pair);route=routes[pair]
    values=(entry,exit,floor,buildings.union(other),route,network.vehicle,network.curbs['arc_error_m'])
    selected,trials=construct_route_roadbed(*values)
    legacy,note=construct_fitted_roadbed(*values)
    def brief(data):
        if isinstance(data,dict):return {k:brief(v) for k,v in data.items() if k in
            ('reason','residual_calls','fit_trials','spacing_m','search_reach_m','trials','optimizer_status','nodes')}
        if isinstance(data,list):return [brief(v) for v in data]
        return data
    row=dict(module=module,pair=pair,heading_dot=float(entry['heading']@exit['heading']),
             source_length_m=route.length,chord_m=float(np.linalg.norm(exit['position']-entry['position'])),
             selected=selected is not None,legacy=legacy is not None,selected_trials=brief(trials),legacy_fit=brief(note))
    row['wide_first']=args.wide_first
    rows.append(row);print(json.dumps(row),flush=True)
output=root/'roadbeds/window-return-budget'/name;output.mkdir(parents=True,exist_ok=True)
(output/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
