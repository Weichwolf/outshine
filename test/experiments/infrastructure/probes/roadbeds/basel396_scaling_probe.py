from pathlib import Path
import inspect,json,sys,time
import numpy as np,shapely
sys.path.insert(0,'test/experiments/infrastructure')
import infrastructure_hermite_route_fitting as fitting
from infrastructure_fitted_roadbeds import construct_fitted_roadbed
from infrastructure_turn_roadbed_experiment import building_reservations,reservations,mesh_coverage
exec(Path('test/experiments/infrastructure/probes/roadbeds/basel396_endpoints.py').read_text().split('rows=[]')[0])
source=inspect.getsource(fitting.fit_hermite_route)
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
region=shape.envelope.buffer(12);buildings,_=building_reservations(receipt,region)
reserved=buildings.union(reservations(net,region).difference(floor))
rows=[]
for scale in (1.,'jac'):
 ns=dict(fitting.__dict__)
 text=source.replace('max_nfev=maximum_evaluations,',f'x_scale={scale!r},max_nfev=maximum_evaluations,')
 exec(text,ns);fitting.fit_hermite_route=ns['fit_hermite_route']
 for pair in ((24,20),(24,21),(24,26),(24,28),(24,30),(24,31)):
  began=time.perf_counter()
  candidate,note=construct_fitted_roadbed(poses[pair[0]],poses[pair[1]],floor,reserved,routes[pair],net.vehicle,net.curbs['arc_error_m'])
  row=dict(pair=pair,scale=scale,accepted=candidate is not None,calls=note['residual_calls'],elapsed_s=time.perf_counter()-began)
  if candidate is not None:
   mesh_coverage(candidate['surface'],net.vehicle,candidate['pieces'],net.curbs['arc_error_m'])
   row['area_m2']=candidate['addition'].area
  rows.append(row);print(json.dumps(row),flush=True)
Path('build/experiments/infrastructure/roadbeds/basel396_scaling_probe.json').write_text(json.dumps(rows,indent=2)+'\n')
