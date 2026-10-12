from pathlib import Path
import inspect,json,sys,time
import numpy as np
sys.path.insert(0,'test/experiments/transport')
import transport_spline_routes as splines
from transport_fitted_roadbeds import construct_fitted_roadbed
from transport_turn_roadbed_experiment import building_reservations,reservations,mesh_coverage
exec(Path('test/experiments/transport/probes/roadbeds/coupled_deficit_probe.py').read_text().split('for module,pair in')[0])
import subprocess
original_source=subprocess.check_output(['git','show','5c76be01c^:test/experiments/transport_spline_routes.py'],text=True)
namespace=dict(splines.__dict__)
exec(compile(original_source,'baseline-spline-routes','exec'),namespace)
original=namespace['curve_bounds']
variant=splines.curve_bounds
modules=[module for name,module in sys.modules.items() if name.startswith('transport_') and getattr(module,'curve_bounds',None) is variant]
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
rows=[]
for module,pair in ((1453,(3,1)),(1565,(4,5)),(1565,(4,8)),(1704,(0,5)),(1262,(1,2)),(1849,(4,2))):
 poses,_=net.poses(module,{})
 routes,_=directed_module_routes(net.graph,poses,net.shape(module),net.tree)
 route=routes[pair];error=net.curbs['arc_error_m']
 floor=motor_surface(route.envelope,net.graph,net.tree,net.mesh,net.vehicle,net.curbs['corner_radius_m'],error)
 buildings,_=building_reservations(receipt,route.envelope.buffer(12))
 reserved=buildings.union(reservations(net,route.envelope.buffer(12)).difference(floor))
 results=[];timings=[[],[]]
 for repeat in range(3):
  for index in ((0,1) if repeat%2==0 else (1,0)):
   for target in modules:target.curve_bounds=(original,variant)[index]
   began=time.perf_counter()
   candidate,note=construct_fitted_roadbed(poses[pair[0]],poses[pair[1]],floor,reserved,route,net.vehicle,error)
   timings[index].append(time.perf_counter()-began)
   signature=dict(accepted=candidate is not None,note=note)
   if candidate is not None:
    signature.update(surface=candidate['surface'].wkb_hex,controls=[p['fit']['controls'] for p in candidate['pieces']])
   results.append(json.dumps(signature,sort_keys=True,default=lambda a:a.tolist()))
 assert len(set(results))==1,(module,pair)
 old,batch=map(lambda t:float(np.median(t)),timings)
 row=dict(module=module,pair=pair,old_s=old,batch_s=batch,speedup=old/batch,identical=True)
 rows.append(row);print(json.dumps(row),flush=True)
Path('build/experiments/transport/roadbeds/bernstein_real_probe.json').write_text(json.dumps(rows,indent=2)+'\n')
