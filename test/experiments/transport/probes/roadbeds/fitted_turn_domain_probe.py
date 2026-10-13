import json,sys,time
from pathlib import Path
import numpy as np,shapely
sys.path.insert(0,'test/experiments/transport')
from transport_cut_setback_experiment import prepared_network
from transport_cut_setbacks import restore_cut_plan
from transport_directed_routes import directed_module_routes
from transport_junction_surfaces import motor_surface
from transport_hermite_route_fitting import fitted_route_candidates
from transport_turn_roadbed_experiment import building_reservations,reservations
from transport_turn_roadbeds import roadbed_lateral_reach

root=Path('build/experiments/transport'); name='ZuerichHauptbahnhof'
before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
network=prepared_network(root,before,next(r['frames'] for r in frames['places'] if r['place']==name))
restore_cut_plan(network,json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text())['plan'])
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
reach=roadbed_lateral_reach(network.vehicle); error=network.curbs['arc_error_m']; results=[]
for module,pair in ((1453,(3,1)),(1565,(4,5)),(1565,(4,8)),(1704,(0,5)),(1704,(3,5))):
    poses,_=network.poses(module,{})
    routes,_=directed_module_routes(network.graph,poses,network.shape(module),network.tree)
    route=routes[pair]
    region=route.envelope.buffer(reach+network.vehicle['frontFromRearAxleM']+float(network.graph['half_widths'].max()))
    floor=motor_surface(route.envelope,network.graph,network.tree,network.mesh,network.vehicle,
                        network.curbs['corner_radius_m'],error)
    buildings,_=building_reservations(receipt,region)
    reserved=buildings.union(reservations(network,region).difference(floor))
    protected=shapely.buffer(reserved.difference(floor),2*error,quad_segs=8)
    for margin in (0.,network.vehicle['widthM']*.5,reach):
        domain=floor if margin==0 else floor.union(shapely.buffer(floor,margin-2*error,quad_segs=8).difference(protected))
        started=time.perf_counter()
        path,note=fitted_route_candidates(poses[pair[0]],poses[pair[1]],route,domain,network.vehicle,error)
        row=dict(module=module,pair=pair,search_reach_m=margin,accepted=path is not None,
                 elapsed_ms=(time.perf_counter()-started)*1000,note=note,
                 path=None if path is None else shapely.get_coordinates(path).tolist())
        results.append(row)
        print(json.dumps({k:row[k] for k in ('module','pair','search_reach_m','accepted','elapsed_ms')}|
                         {k:note[k] for k in ('reason','residual_calls','minimum_radius_m') if k in note}),flush=True)
Path('build/experiments/transport/roadbeds/fitted-turn-domain-results.json').write_text(
    json.dumps(dict(vehicle=network.vehicle,results=results,scope='Curve candidates on bounded solve domains; no published roadbed or runtime proof.'),
               indent=2,default=lambda obj:obj.tolist())+'\n')
