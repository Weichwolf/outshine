from pathlib import Path
import inspect,json,sys,time

import numpy as np
import shapely

sys.path.insert(0,'test/experiments/infrastructure')
import infrastructure_hermite_route_fitting as fitting
import infrastructure_fitted_roadbeds as beds
from infrastructure_turn_roadbed_experiment import building_reservations,reservations,mesh_coverage
from infrastructure_turn_roadbeds import compose_roadbeds,preserves_voids

exec(Path('test/experiments/infrastructure/probes/roadbeds/coupled_deficit_probe.py').read_text().split('for module,pair in')[0])
fit_source=inspect.getsource(fitting.fit_hermite_route)
candidate_source=inspect.getsource(fitting.fitted_route_candidates)
assert candidate_source.count('(spacing,False)')==1
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
rows=[]
for module,pair in ((1453,(3,1)),(1565,(4,5)),(1262,(1,2)),(1849,(4,2)),(1565,(4,8)),(1704,(0,5))):
 poses,_=net.poses(module,{})
 routes,_=directed_module_routes(net.graph,poses,net.shape(module),net.tree)
 route=routes[pair];error=net.curbs['arc_error_m'];shape=net.shape(module)
 floor=motor_surface(shape,net.graph,net.tree,net.mesh,net.vehicle,net.curbs['corner_radius_m'],error)
 buildings,_=building_reservations(receipt,route.envelope.buffer(20))
 reserved=buildings.union(reservations(net,route.envelope.buffer(20)).difference(floor))
 for weight in (4,16):
  namespace=dict(fitting.__dict__)
  exec(fit_source.replace('bends = 4*',f'bends = {weight}*'),namespace)
  exec(candidate_source.replace('(spacing,False)','(spacing,True)'),namespace)
  beds.fitted_route_candidates=namespace['fitted_route_candidates']
  began=time.perf_counter()
  candidate,note=beds.construct_fitted_roadbed(poses[pair[0]],poses[pair[1]],floor,reserved,route,net.vehicle,error)
  row=dict(module=module,pair=pair,weight=weight,accepted=candidate is not None,
           residual_calls=note['residual_calls'],elapsed_ms=(time.perf_counter()-began)*1000)
  if candidate is not None:
   mesh_coverage(candidate['surface'],net.vehicle,candidate['pieces'],error)
   _,core,_,physical,_=compose_roadbeds(floor,shape,[candidate['roadbed']])
   row.update(added_area_m2=candidate['addition'].area,physical_voids_preserved=preserves_voids(core,physical),
              native_body_proved=True,minimum_radius_m=note['minimum_radius_m'])
  else:row['reasons']=[t.get('reason') for t in note['trials']]
  rows.append(row)
  print(json.dumps(row),flush=True)
Path('build/experiments/infrastructure/roadbeds/hermite_body_sampling_probe.json').write_text(json.dumps(rows,indent=2)+'\n')
