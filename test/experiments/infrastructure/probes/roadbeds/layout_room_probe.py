from pathlib import Path
import inspect,json,sys,time
import numpy as np
import shapely
sys.path.insert(0,'test/experiments/infrastructure')
import infrastructure_hermite_route_fitting as fitting
import infrastructure_fitted_roadbeds as beds
import infrastructure_turn_roadbeds as sites
from infrastructure_turn_roadbed_experiment import building_reservations,reservations,mesh_coverage
exec(Path('test/experiments/infrastructure/probes/roadbeds/coupled_deficit_probe.py').read_text().split('for module,pair in')[0])
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
fit_source=inspect.getsource(fitting.fit_hermite_route)
bed_source=inspect.getsource(beds.construct_fitted_roadbed).replace("sorted({vehicle['widthM']*.5,roadbed_lateral_reach(vehicle)})",'[maximum_reach]')
site_source=inspect.getsource(sites.roadbed_site_check)
rows=[]
for module,pair in ((1453,(3,1)),(1565,(4,5))):
 poses,_=net.poses(module,{})
 routes,_=directed_module_routes(net.graph,poses,net.shape(module),net.tree)
 route=routes[pair];error=net.curbs['arc_error_m'];shape=net.shape(module)
 floor=motor_surface(route.envelope,net.graph,net.tree,net.mesh,net.vehicle,net.curbs['corner_radius_m'],error)
 buildings,_=building_reservations(receipt,route.envelope.buffer(20))
 reserved=buildings.union(reservations(net,route.envelope.buffer(20)).difference(floor))
 for reach in (1.907622195,3.,4.,6.):
  site_ns=dict(sites.__dict__,roadbed_lateral_reach=lambda vehicle:reach)
  exec(site_source,site_ns)
  bed_ns=dict(beds.__dict__,maximum_reach=reach,roadbed_site_check=site_ns['roadbed_site_check'])
  exec(bed_source,bed_ns)
  for weight in (4,16):
   fit_ns=dict(fitting.__dict__)
   exec(fit_source.replace('bends = 4*',f'bends = {weight}*'),fit_ns)
   fitting.fit_hermite_route=fit_ns['fit_hermite_route']
   began=time.perf_counter()
   candidate,note=bed_ns['construct_fitted_roadbed'](poses[pair[0]],poses[pair[1]],floor,reserved,route,net.vehicle,error)
   row=dict(module=module,pair=pair,reach_m=reach,weight=weight,accepted=candidate is not None,calls=note['residual_calls'],elapsed_s=time.perf_counter()-began)
   if candidate is not None:
    mesh_coverage(candidate['surface'],net.vehicle,candidate['pieces'],error)
    _,core,_,physical,_=sites.compose_roadbeds(floor,shape,[candidate['roadbed']])
    row.update(area_m2=candidate['addition'].area,voids_preserved=sites.preserves_voids(core,physical),min_radius_m=note['minimum_radius_m'])
   else:row['reasons']=[t.get('reason') for t in note['trials']]
   rows.append(row);print(json.dumps(row),flush=True)
Path('build/experiments/infrastructure/roadbeds/layout_room_probe.json').write_text(json.dumps(rows,indent=2)+'\n')
