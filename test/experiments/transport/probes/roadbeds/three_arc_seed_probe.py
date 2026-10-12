from pathlib import Path
import json,sys,time
import numpy as np
import shapely
sys.path.insert(0,'test/experiments/transport')
from transport_hermite_route_fitting import fitted_route_candidates
from transport_fitted_roadbeds import spline_roadbed
from transport_turn_roadbed_experiment import building_reservations,reservations,mesh_coverage
from transport_turn_roadbeds import roadbed_site_check,geometry_precision
from transport_junction_surfaces import same_island_winding
from transport_spline_routes import checked_spline_path
exec(Path('test/experiments/transport/probes/roadbeds/coupled_deficit_probe.py').read_text().split('for module,pair in')[0])

from transport_three_arc_paths import three_arc_seeds as seeds

receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt'];rows=[]
for module,pair in ((1453,(3,1)),(1565,(4,5))):
 poses,_=net.poses(module,{})
 routes,_=directed_module_routes(net.graph,poses,net.shape(module),net.tree);route=routes[pair]
 error=net.curbs['arc_error_m'];floor=motor_surface(route.envelope,net.graph,net.tree,net.mesh,net.vehicle,net.curbs['corner_radius_m'],error)
 buildings,_=building_reservations(receipt,route.envelope.buffer(20))
 reserved=buildings.union(reservations(net,route.envelope.buffer(20)).difference(floor))
 domain=floor.union(floor.buffer(.9-2*error).difference(reserved.difference(floor).buffer(2*error)))
 for radius in (4.5,5.):
  for kind,seed in seeds(poses[pair[0]],poses[pair[1]],radius):
   row=dict(module=module,pair=pair,radius=radius,kind=kind,length=seed.length)
   if not same_island_winding(seed,route,floor):
    row.update(accepted=False,reason='seed_changes_winding')
   else:
    began=time.perf_counter()
    path,note=fitted_route_candidates(poses[pair[0]],poses[pair[1]],seed,domain,net.vehicle,error,include_controls=True)
    row.update(calls=note['residual_calls'],elapsed_s=time.perf_counter()-began,accepted=False,reason=note.get('reason'))
    if path is not None:
     controls=np.array(note['curve_controls']);bed,audit=spline_roadbed(controls,net.vehicle,error)
     if bed is not None:
      base,protect,bed=shapely.set_precision(np.array([floor,reserved,bed],dtype=object),geometry_precision(floor))
      planned,addition,reason=roadbed_site_check(bed.difference(protect),base,protect,net.vehicle,error)
      if reason is None:
       path,proof=checked_spline_path(controls,planned,net.vehicle,error)
       row.update(accepted=path is not None and same_island_winding(path,route,floor),reason=proof.get('reason'),area_m2=addition.area)
      else:row['reason']=reason
     else:row['reason']=audit['reason']
   rows.append(row);print(json.dumps(row),flush=True)
Path('build/experiments/transport/roadbeds/three_arc_seed_probe.json').write_text(json.dumps(rows,indent=2)+'\n')
