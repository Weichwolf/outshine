import json,sys,time
from pathlib import Path
import numpy as np
import shapely
sys.path.insert(0,'test/experiments/infrastructure')
from infrastructure_turn_roadbed_experiment import prepared_network,building_reservations,reservations,motor_surface,directed_module_routes
from infrastructure_cut_setbacks import restore_cut_plan
from infrastructure_source_roadbeds import construct_route_roadbed

root=Path('build/experiments/infrastructure');name='Wien';module=726
before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
cuts=json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text());restore_cut_plan(network,cuts['plan'])
shape=network.shape(module);poses,_=network.poses(module,{})
floor=motor_surface(shape,network.graph,network.tree,network.mesh,network.vehicle,network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
bounds=shapely.MultiPoint(network.product['vertices']).envelope.buffer(network.vehicle['widthM'])
start=time.perf_counter();buildings,tiles=building_reservations(receipt,bounds);other=reservations(network,bounds)
global_reserved=buildings.union(other)
region=floor.envelope.buffer(network.vehicle['widthM']*.5)
local_buildings,local_tiles=building_reservations(receipt,region)
local_reserved=local_buildings.union(reservations(network,region).difference(floor))
print(json.dumps(dict(stage='reservations',elapsed_ms=(time.perf_counter()-start)*1000,
    global_coordinates=int(shapely.get_num_coordinates(global_reserved)),local_coordinates=int(shapely.get_num_coordinates(local_reserved)))),flush=True)
routes,_=directed_module_routes(network.graph,poses,shape,network.tree)
for pair in ((0,16),(18,16)):
    start=time.perf_counter()
    candidate,notes=construct_route_roadbed(poses[pair[0]],poses[pair[1]],floor,local_reserved,routes[pair],network.vehicle)
    print(json.dumps(dict(pair=pair,local_accepted=candidate is not None,elapsed_ms=(time.perf_counter()-start)*1000,
        global_reserved_overlap_m2=candidate['addition'].intersection(global_reserved).area if candidate else None)),flush=True)
for label,reserve in [('global',global_reserved),('localized',global_reserved.intersection(region).difference(floor))]:
    start=time.perf_counter();protected=shapely.buffer(reserve,.05,quad_segs=8)
    domain=floor.union(shapely.buffer(floor,.85,quad_segs=8).difference(protected))
    print(json.dumps(dict(stage='search_domain',label=label,elapsed_ms=(time.perf_counter()-start)*1000,
        coordinates=int(shapely.get_num_coordinates(domain)),area_m2=domain.area)),flush=True)
