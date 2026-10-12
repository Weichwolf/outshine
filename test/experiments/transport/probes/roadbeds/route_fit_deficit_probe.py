"""Inspect a missing route on an already published joint roadbed."""

import argparse
import json
from pathlib import Path
import sys
import time

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from transport_cut_setback_experiment import prepared_network
from transport_cut_setbacks import restore_cut_plan
from transport_coupled_roadbed_products import apply_coupled_product
from transport_directed_routes import directed_module_routes
from transport_hermite_route_fitting import fitted_route_candidates
from transport_junction_surfaces import motor_surface
from transport_source_roadbeds import construct_route_roadbed
from transport_turn_roadbed_experiment import building_reservations,reservations
from transport_vehicle_sweep import body_outlines

parser=argparse.ArgumentParser(description=__doc__)
source=parser.add_mutually_exclusive_group(required=True)
source.add_argument('--product',type=Path)
source.add_argument('--place')
parser.add_argument('--module',type=int,required=True)
parser.add_argument('--pair',type=int,nargs=2,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--snapshot-only',action='store_true')
args=parser.parse_args();root=Path('build/experiments/transport')
report=json.loads(args.product.read_text()) if args.product else dict(place=args.place)
name=report['place']
before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
cuts=json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text());restore_cut_plan(network,cuts['plan'])
records={r['module']:r for r in before['modules']};records.update((r['module'],r) for r in cuts['changed_modules'])
if args.product:network,_,_,_=apply_coupled_product(network,records,set(),root,args.product,set())
poses,_=network.poses(args.module,{})
entry,exit=(poses[i] for i in args.pair)
routes,_=directed_module_routes(network.graph,poses,network.shape(args.module),network.tree)
route=routes[tuple(args.pair)]
published=next((p for p in report.get('published_surfaces',[]) if p['module']==args.module),None)
surface=(shapely.from_wkb(published['public_surface_wkb']) if published is not None else
         motor_surface(network.shape(args.module),network.graph,network.tree,network.mesh,network.vehicle,
                       network.curbs['corner_radius_m'],network.curbs['arc_error_m']))
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
buildings,_=building_reservations(receipt,surface.envelope.buffer(10))
reserved=buildings.union(reservations(network,surface))
xy=np.array([p['position'] for p in (entry,exit)]);heading=np.array([np.arctan2(p['heading'][1],p['heading'][0]) for p in (entry,exit)])
bodies=shapely.polygons(body_outlines(xy,heading,network.vehicle['widthM']*.5,
                     network.vehicle['frontFromRearAxleM'],network.vehicle['rearOverhangM']))
print(json.dumps(dict(length_m=route.length,points=len(shapely.get_coordinates(route)),
                     endpoint_outside_m2=shapely.area(shapely.difference(bodies,surface)).tolist())),flush=True)
args.output.mkdir(parents=True,exist_ok=True)
snapshot=dict(entry={k:entry[k].tolist() for k in ('position','heading')},
              exit={k:exit[k].tolist() for k in ('position','heading')},surface=surface.wkb.hex(),
              reservations=reserved.wkb.hex(),source_route=route.wkb.hex(),vehicle=network.vehicle,
              buildings=buildings.wkb.hex(),non_motor_reservations=reservations(network,surface).wkb.hex(),
              error_m=network.curbs['arc_error_m'])
(args.output/'input.json').write_text(json.dumps(snapshot)+'\n')
if args.snapshot_only:raise SystemExit(0)
began=time.perf_counter()
candidate,trials=construct_route_roadbed(entry,exit,surface,reserved,route,network.vehicle,network.curbs['arc_error_m'])
path,fit=fitted_route_candidates(entry,exit,route,surface,network.vehicle,network.curbs['arc_error_m'])
def brief(value):
    if isinstance(value,dict):return {k:brief(v) for k,v in value.items() if k in
        ('reason','residual_calls','maximum_residual_calls','search_reach_m','trials','fit_trials','spacing_m',
         'nodes','optimizer_status','minimum_objective_seen','profile','vehicle_sweep','fits_surface','outside_poses')}
    if isinstance(value,list):return [brief(v) for v in value]
    return value
result=dict(candidate=candidate is not None,on_existing=path is not None,trials=brief(trials),
            fitting=brief(fit),elapsed_ms=(time.perf_counter()-began)*1000)
(args.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result),flush=True)
