from pathlib import Path
import inspect,json,sys,time
import numpy as np
import shapely
sys.path.insert(0,'test/experiments/infrastructure')
import infrastructure_cut_setbacks as cuts
from infrastructure_fitted_roadbeds import construct_fitted_roadbed
from infrastructure_turn_roadbed_experiment import building_reservations,reservations
exec(Path('test/experiments/infrastructure/probes/roadbeds/coupled_deficit_probe.py').read_text().split('for module,pair in')[0])
source=inspect.getsource(cuts.setback_network).replace('not np.isfinite(distance) or distance<=0','any(not np.isfinite(d) or d<=0 for d in distance.values())').replace('displaced_source_section(network,pose,distance)','displaced_source_section(network,pose,distance[section])')
ns=dict(cuts.__dict__);exec(source,ns)
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
rows=[]
for module,pair,distances in ((1453,(3,1),((4,2),(8,2),(12,2),(16,3))), (1565,(4,5),((4,8),(8,4),(8,12),(12,8)))):
 original,_=net.poses(module,{})
 keys=[(original[i]['section'],original[i]['edge'],original[i]['direction']) for i in pair]
 for offsets in distances:
  began=time.perf_counter();row=dict(module=module,pair=pair,offsets_m=offsets)
  try:
   shifts=dict(zip((k[0] for k in keys),offsets))
   trial,changes,affected=ns['setback_network'](net,module,set(shifts),shifts)
   poses,_=trial.poses(module,{})
   ids={(p['section'],p['edge'],p['direction']):i for i,p in enumerate(poses)}
   selected=tuple(ids[k] for k in keys)
   routes,_=directed_module_routes(trial.graph,poses,trial.shape(module),trial.tree)
   route=routes[selected];error=trial.curbs['arc_error_m']
   floor=motor_surface(route.envelope,trial.graph,trial.tree,trial.mesh,trial.vehicle,trial.curbs['corner_radius_m'],error)
   region=route.envelope.buffer(12)
   buildings,_=building_reservations(receipt,region)
   reserved=buildings.union(reservations(trial,region).difference(floor))
   candidate,note=construct_fitted_roadbed(poses[selected[0]],poses[selected[1]],floor,reserved,route,trial.vehicle,error)
   row.update(accepted=candidate is not None,reason=note.get('reason'),calls=note['residual_calls'])
  except (AssertionError,cuts.CutSetbackRejected) as error:row.update(accepted=False,reason=str(error))
  row['elapsed_s']=time.perf_counter()-began
  rows.append(row);print(json.dumps(row),flush=True)
Path('build/experiments/infrastructure/roadbeds/unequal_setbacks_probe.json').write_text(json.dumps(rows,indent=2)+'\n')
