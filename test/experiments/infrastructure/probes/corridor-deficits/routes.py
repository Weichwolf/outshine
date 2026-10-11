from pathlib import Path
import json,numpy as np,shapely
import infrastructure_corridor_experiment as c
from infrastructure_directed_routes import directed_module_routes
from infrastructure_curvature_paths import fit_curvature_paths
from infrastructure_spline_routes import spline_route
v=json.loads(Path('test/experiments/infrastructure/infrastructure_network_recipes.json').read_text())['vehicleProfiles']['compact']
original=c.solve_module;views=[]
def inspect(module,shape,poses,unused,graph,tree,mesh,vehicle,curbs,**kw):
 r=original(module,shape,poses,unused,graph,tree,mesh,vehicle,curbs,**kw)
 if r[0]['accepted']==r[0]['connections']:return r
 routes,_=directed_module_routes(graph,poses,shape,tree)
 for t in r[0]['turns']:
  if t['accepted']:continue
  i,j=t['entry'],t['exit'];a,b=poses[i],poses[j];route=routes[i,j]
  angles=[np.arctan2(p['heading'][1],p['heading'][0]) for p in (a,b)]
  fit=fit_curvature_paths(a['position'][None,:],b['position'][None,:],np.array([angles[0]]),np.array([angles[1]]))
  print(name,module,i,j,'length',route.length,'points',shapely.get_num_coordinates(route),'headings',angles,'poly_fit',{k:fit[k].tolist() for k in ('valid','length_m','maximum_curvature_per_m')},flush=True)
  for spacing in (8.,4.,12.,16.,None):
   _,note=spline_route(a,b,route,r[1][1],v,spacing=spacing);print('spline',spacing,note,flush=True)
 views.append((name,module,r[0],r[1]))
 return r
c.solve_module=inspect
for name in ('Wien','ZuerichHauptbahnhof'):
 c.survey(Path('build/experiments/infrastructure'),name,v,96)
c.render(Path('build/shots/experiments/infrastructure/corridor-deficits.png'),views[:6])
