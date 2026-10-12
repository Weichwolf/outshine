from pathlib import Path
import inspect,json,sys,time

import numpy as np
import shapely

sys.path.insert(0,'test/experiments/transport')
import transport_hermite_route_fitting as fitting
from transport_fitted_roadbeds import construct_fitted_roadbed
from transport_turn_roadbed_experiment import building_reservations,reservations,mesh_coverage
from transport_turn_roadbeds import compose_roadbeds,preserves_voids

exec(Path('test/experiments/transport/probes/roadbeds/coupled_deficit_probe.py').read_text().split('for module,pair in')[0])
original=fitting.fit_hermite_route
source=inspect.getsource(original)
assert source.count('bends = 4*')==1
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
rows=[]
for module,pair in ((1453,(3,1)),(1565,(4,5)),(1565,(4,8)),(1704,(0,5))):
 poses,_=net.poses(module,{})
 routes,_=directed_module_routes(net.graph,poses,net.shape(module),net.tree)
 route=routes[pair];error=net.curbs['arc_error_m'];shape=net.shape(module)
 floor=motor_surface(route.envelope,net.graph,net.tree,net.mesh,net.vehicle,net.curbs['corner_radius_m'],error)
 buildings,_=building_reservations(receipt,route.envelope.buffer(12))
 reserved=buildings.union(reservations(net,route.envelope.buffer(12)).difference(floor))
 for weight in (4,8,16,32):
  namespace=dict(fitting.__dict__)
  exec(compile(source.replace('bends = 4*',f'bends = {weight}*'),str(Path(__file__).resolve())+f':weight-{weight}','exec'),namespace)
  fitting.fit_hermite_route=namespace['fit_hermite_route']
  began=time.perf_counter()
  candidate,note=construct_fitted_roadbed(poses[pair[0]],poses[pair[1]],floor,reserved,route,net.vehicle,error)
  row=dict(module=module,pair=pair,weight=weight,accepted=candidate is not None,
           residual_calls=note['residual_calls'],elapsed_ms=(time.perf_counter()-began)*1000)
  if candidate is not None:
   mesh_coverage(candidate['surface'],net.vehicle,candidate['pieces'],error)
   _,core,_,physical,_=compose_roadbeds(floor,shape,[candidate['roadbed']])
   row.update(added_area_m2=candidate['addition'].area,physical_voids_preserved=preserves_voids(core,physical),
              native_body_proved=True,minimum_radius_m=note['minimum_radius_m'])
  else:
   row['reasons']=[t.get('reason') for t in note['trials']]
  rows.append(row)
  print(json.dumps(row),flush=True)
fitting.fit_hermite_route=original
Path('build/experiments/transport/roadbeds/hermite_penalty_probe.json').write_text(json.dumps(rows,indent=2)+'\n')
