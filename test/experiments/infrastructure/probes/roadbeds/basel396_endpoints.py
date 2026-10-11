import json,sys
from pathlib import Path
import numpy as np,shapely
sys.path.insert(0,'test/experiments/infrastructure')
from infrastructure_cut_setback_experiment import prepared_network
from infrastructure_cut_setbacks import restore_cut_plan
from infrastructure_directed_routes import directed_module_routes
from infrastructure_junction_surfaces import motor_surface
from infrastructure_vehicle_sweep import body_outlines
root=Path('build/experiments/infrastructure');name='BaselBadischerBahnhof'
before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text());frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
net=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
restore_cut_plan(net,json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text())['plan'])
poses,_=net.poses(396,{});shape=net.shape(396)
routes,_=directed_module_routes(net.graph,poses,shape,net.tree)
floor=motor_surface(shape,net.graph,net.tree,net.mesh,net.vehicle,net.curbs['corner_radius_m'],net.curbs['arc_error_m'])
rows=[]
for i in (6,9,19,20,21,22,24,26,27,28,30,31):
 p=poses[i];body=shapely.Polygon(body_outlines(p['position'][None,:],np.array([np.arctan2(*p['heading'][::-1])]),net.vehicle['widthM']*.5,net.vehicle['frontFromRearAxleM'],net.vehicle['rearOverhangM'])[0])
 row=dict(port=i,position=p['position'].tolist(),outside_m2=body.difference(floor).area,section=p['section'],corridor=p['corridor'],edge=p['edge'],station=p['station'],length=net.graph['lines'][p['edge']].length,incoming=p['incoming'],direction=p['direction'])
 rows.append(row);print(json.dumps(row))
Path('build/experiments/infrastructure/roadbeds/basel396_endpoints.json').write_text(json.dumps(rows,indent=2)+'\n')
