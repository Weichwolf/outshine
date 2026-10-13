from pathlib import Path
import json,sys,time
import numpy as np
import shapely
from scipy.optimize import least_squares
from scipy.sparse import lil_matrix
sys.path.insert(0,'test/experiments/transport')
from transport_hermite_route_fitting import hermite_controls,ResidualBudgetExceeded
from transport_spline_routes import evaluate_controls,split_controls,curve_bounds,checked_spline_path
from transport_surface_clearance import SurfaceClearance
from transport_turn_roadbed_experiment import building_reservations,reservations
from transport_turn_roadbeds import roadbed_lateral_reach
exec(Path('test/experiments/transport/probes/roadbeds/coupled_deficit_probe.py').read_text().split('for module,pair in')[0])

def solve(poses,routes,pairs,movable,sections,surface,vehicle,error,maximum_calls=4096):
 states=[];slices=[];initial=[0.]*(2*len(movable));steps=[]
 for pair in pairs:
  route=routes[pair];count=max(3,min(18,int(np.ceil(route.length/8))+1))
  raw=shapely.get_coordinates(shapely.line_interpolate_point(route,np.linspace(0,route.length,count)))
  raw[[0,-1]]=[poses[i]['position'] for i in pair]
  state=np.zeros((count,6));state[:,:2]=raw
  state[1:-1,2:4]=(raw[2:]-raw[:-2])/2;state[1:-1,4:]=raw[2:]-2*raw[1:-1]+raw[:-2]
  step=route.length/(count-1);steps.append(step)
  local=np.r_[state[1:-1].ravel(),step,step]
  slices.append(slice(len(initial),len(initial)+len(local)));initial.extend(local);states.append(state)
 initial=np.array(initial);lower=np.full(len(initial),-np.inf);upper=-lower
 for i,p in enumerate(movable):
  ends=shapely.get_coordinates(sections[poses[p]['section']]);axis=ends[-1]-ends[0];axis/=np.linalg.norm(axis)
  lower[2*i],upper[2*i]=-1.,1.;lower[2*i+1],upper[2*i+1]=-.3,.3
 for sl,step in zip(slices,steps):lower[sl.stop-2:sl.stop]=step*.1
 def decode(values):
  frames=[dict(p) for p in poses]
  for i,p in enumerate(movable):
   ends=shapely.get_coordinates(sections[poses[p]['section']]);axis=ends[-1]-ends[0];axis/=np.linalg.norm(axis)
   angle=np.arctan2(*poses[p]['heading'][::-1])+values[2*i+1]
   frames[p]=dict(poses[p],position=poses[p]['position']+axis*values[2*i],heading=np.array([np.cos(angle),np.sin(angle)]))
  curves=[]
  for state,sl,pair in zip(states,slices,pairs):
   current=state.copy();v=values[sl];current[1:-1]=v[:-2].reshape(-1,6)
   current[0,:2],current[-1,:2]=[frames[p]['position'] for p in pair]
   current[0,2:4]=v[-2]*frames[pair[0]]['heading'];current[-1,2:4]=v[-1]*frames[pair[1]]['heading']
   curves.append(hermite_controls(current))
  return frames,curves
 clearance=SurfaceClearance(surface);calls=0;best=initial.copy();best_cost=np.inf
 def residual(values):
  nonlocal calls,best,best_cost
  if calls>=maximum_calls:raise ResidualBudgetExceeded
  calls+=1;_,curves=decode(values);parts=[]
  for controls,step in zip(curves,steps):
   count=len(controls);owners=np.repeat(np.arange(count),17);stations=np.tile(np.linspace(0,1,17),count)
   xy=evaluate_controls(controls[owners],stations);velocity=evaluate_controls(5*np.diff(controls[owners],axis=1),stations)
   refined=controls
   for _ in range(3):refined=split_controls(refined)
   _,_,curvature=curve_bounds(refined);curvature=np.nan_to_num(curvature,nan=1e6,posinf=1e6)
   bodies=body_outlines(xy,np.arctan2(velocity[:,1],velocity[:,0]),vehicle['widthM']*.5,vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
   shortfall=clearance.shortfall(shapely.points(bodies.reshape(-1,2)),error*3).reshape(count,68)
   parts.append(np.c_[4*shortfall,4*np.maximum(curvature.reshape(count,8)*vehicle['minimumRearAxleRadiusM']-.96,0),np.maximum(.1-np.linalg.norm(velocity,axis=1).reshape(count,17)/step,0)].ravel())
  result=np.concatenate([*parts,.005*(values-initial)]);cost=float(np.sum(result**2))
  if cost<best_cost:best_cost=cost;best=values.copy()
  return result
 rows=sum(93*(len(s)-1) for s in states);pattern=lil_matrix((rows+len(initial),len(initial)),dtype=np.uint8);offset=0
 for state,sl,pair in zip(states,slices,pairs):
  for segment in range(len(state)-1):
   row=slice(offset+93*segment,offset+93*(segment+1))
   for node in (segment,segment+1):
    if 0<node<len(state)-1:pattern[row,sl.start+6*(node-1):sl.start+6*node]=1
   for end,p in enumerate(pair):
    if segment==(0 if end==0 else len(state)-2):
     pattern[row,sl.stop-2+end]=1
     if p in movable:pattern[row,2*movable.index(p):2*movable.index(p)+2]=1
  offset+=93*(len(state)-1)
 for i in range(len(initial)):pattern[rows+i,i]=1
 began=time.perf_counter()
 try:least_squares(residual,initial,bounds=(lower,upper),jac_sparsity=pattern.tocsr(),max_nfev=100,ftol=1e-5,xtol=1e-6,gtol=1e-5)
 except ResidualBudgetExceeded:pass
 frames,curves=decode(best);proofs=[]
 for pair,controls in zip(pairs,curves):
  for _ in range(3):controls=split_controls(controls)
  path,proof=checked_spline_path(controls,surface,vehicle,error)
  proofs.append(dict(pair=pair,accepted=path is not None,reason=proof.get('reason'),curvature=proof.get('curvature_bound_per_m')))
 return dict(calls=calls,elapsed_s=time.perf_counter()-began,cost=best_cost,port_parameters=best[:2*len(movable)].tolist(),proofs=proofs)

module=1565;poses,_=net.poses(module,{})
routes,_=directed_module_routes(net.graph,poses,net.shape(module),net.tree)
region=shapely.union_all(list(routes.values())).envelope
floor=motor_surface(region,net.graph,net.tree,net.mesh,net.vehicle,net.curbs['corner_radius_m'],net.curbs['arc_error_m'])
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
buildings,_=building_reservations(receipt,region.buffer(12));reserved=buildings.union(reservations(net,region.buffer(12)).difference(floor))
error=net.curbs['arc_error_m'];expanded=floor.buffer(.9-2*error).difference(reserved.difference(floor).buffer(2*error)).union(floor)
rows=[]
for pairs,movable in [([(4,5),(4,8)],[4]), ([(4,5),(4,8)],[4,5,8])]:
 row=solve(poses,routes,pairs,movable,net.sections,expanded,net.vehicle,error)
 row['movable']=movable;rows.append(row);print(json.dumps(row),flush=True)
Path('build/experiments/transport/roadbeds/joint_port_fit_probe.json').write_text(json.dumps(rows,indent=2)+'\n')
