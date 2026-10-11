from pathlib import Path
import json,sys,time
import numpy as np,shapely
sys.path.insert(0,'test/experiments/infrastructure')
from infrastructure_cut_setbacks import setback_network,CutSetbackRejected
from infrastructure_source_roadbeds import construct_route_roadbed
from infrastructure_turn_roadbed_experiment import building_reservations,reservations
exec(Path('test/experiments/infrastructure/probes/roadbeds/basel396_endpoints.py').read_text().split('rows=[]')[0])
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt'];rows=[]
for distance in (4.,8.,12.):
 began=time.perf_counter()
 try:
  trial,changes,affected=setback_network(net,396,{poses[24]['section']},distance)
  shifted,_=trial.poses(396,{})
  rr,_=directed_module_routes(trial.graph,shifted,trial.shape(396),trial.tree)
  floor=motor_surface(trial.shape(396),trial.graph,trial.tree,trial.mesh,trial.vehicle,trial.curbs['corner_radius_m'],trial.curbs['arc_error_m'])
  region=trial.shape(396).envelope.buffer(12);buildings,_=building_reservations(receipt,region)
  reserved=buildings.union(reservations(trial,region).difference(floor))
  for exit in (20,21,26,28,30,31):
   candidate,notes=construct_route_roadbed(shifted[24],shifted[exit],floor,reserved,rr[24,exit],trial.vehicle,trial.curbs['arc_error_m'])
   row=dict(distance_m=distance,pair=[24,exit],accepted=candidate is not None,elapsed_s=time.perf_counter()-began)
   if candidate is not None:row.update(area_m2=candidate['addition'].area,profile=candidate['report']['profile'])
   rows.append(row);print(json.dumps(row),flush=True)
 except (AssertionError,CutSetbackRejected) as error:
  row=dict(distance_m=distance,accepted=False,reason=str(error));rows.append(row);print(json.dumps(row),flush=True)
Path('build/experiments/infrastructure/roadbeds/basel396_single_cut_probe.json').write_text(json.dumps(rows,indent=2)+'\n')
