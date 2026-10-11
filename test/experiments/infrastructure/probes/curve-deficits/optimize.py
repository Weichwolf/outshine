from pathlib import Path
import json
import sys
import time

import numpy as np
import shapely
from scipy.optimize import least_squares
from scipy.sparse import lil_matrix

sys.path.insert(0,'test/experiments/infrastructure')
from infrastructure_driving_network import DrivingNetwork
from infrastructure_directed_routes import directed_module_routes
from infrastructure_shared_frames import frame_key
from infrastructure_spline_routes import evaluate_controls,curve_bounds,body_acceleration_bounds,split_controls
from infrastructure_vehicle_sweep import body_outlines,swept_hull_check
from infrastructure_junction_surfaces import same_island_winding

ROOT=Path('build/experiments/infrastructure')
OUTPUT=ROOT/'curve-deficits'
VEHICLE=json.loads(Path('test/experiments/infrastructure/infrastructure_network_recipes.json').read_text())['vehicleProfiles']['compact']
REFERENCE=json.loads((ROOT/'pose-frames/shared-frames.json').read_text())


def hermite_controls(state):
    a,b=state[:-1],state[1:]
    return np.stack((a[:,:2],a[:,:2]+a[:,2:4]/5,a[:,:2]+2*a[:,2:4]/5+a[:,4:]/20,
                     b[:,:2]-2*b[:,2:4]/5+b[:,4:]/20,b[:,:2]-b[:,2:4]/5,b[:,:2]),axis=1)


def checked_curve(controls,surface,error):
    for _ in range(3):
        controls=split_controls(controls)
    low,high,curvature=curve_bounds(controls)
    if not np.all(np.isfinite(curvature)) or curvature.max()*VEHICLE['minimumRearAxleRadiusM']>1:
        return None,dict(reason='continuous_curvature_exceeds_vehicle',bound=float(curvature.max()))
    radius=np.hypot(VEHICLE['frontFromRearAxleM'],VEHICLE['widthM']*.5)
    position_bound,body_bound=body_acceleration_bounds(controls,low,high,curvature,radius)
    velocities=5*np.diff(controls,axis=1)
    def evaluate(owners,stations):
        xy=evaluate_controls(controls[owners],stations)
        v=evaluate_controls(velocities[owners],stations)
        return xy,np.arctan2(v[:,1],v[:,0])
    body=swept_hull_check(surface,evaluate,body_bound,VEHICLE,error)
    if not body['fits_surface']:
        return None,dict(reason='continuous_body_rejected',body=body)
    counts=np.maximum(1,np.ceil(np.sqrt(position_bound/(8*error))).astype(int))
    owners=np.repeat(np.arange(len(controls)),counts+1)
    stations=np.concatenate([np.linspace(0,1,n+1) for n in counts])
    xy=evaluate_controls(controls[owners],stations)
    xy=xy[np.r_[True,np.linalg.norm(np.diff(xy,axis=0),axis=1)>1e-10]]
    path=shapely.LineString(xy)
    if not path.is_simple:
        return None,dict(reason='self_intersection')
    return path,dict(reason='accepted',curvature_bound=float(curvature.max()),body=body,vertices=len(xy))


def optimize(entry,exit,route,surface,error,maximum_evaluations=40,spacing=8.):
    started=time.perf_counter()
    count=max(3,min(18,int(np.ceil(route.length/spacing))+1))
    raw=shapely.get_coordinates(shapely.line_interpolate_point(route,np.linspace(0,route.length,count)))
    raw[[0,-1]]=[entry['position'],exit['position']]
    step=route.length/(count-1)
    state=np.zeros((count,6))
    state[:,:2]=raw
    state[1:-1,2:4]=(raw[2:]-raw[:-2])/2
    state[1:-1,4:]=raw[2:]-2*raw[1:-1]+raw[:-2]
    state[0,2:4],state[-1,2:4]=step*entry['heading'],step*exit['heading']
    initial=np.r_[state[1:-1].ravel(),step,step]
    segments=count-1
    owners=np.repeat(np.arange(segments),17)
    stations=np.tile(np.linspace(0,1,17),segments)
    boundary=shapely.boundary(surface)
    shapely.prepare(surface)
    calls=0
    def decode(values):
        result=state.copy()
        result[1:-1]=values[:-2].reshape(-1,6)
        result[0,2:4]=values[-2]*entry['heading']
        result[-1,2:4]=values[-1]*exit['heading']
        return hermite_controls(result)
    def residual(values):
        nonlocal calls
        calls+=1
        controls=decode(values)
        xy=evaluate_controls(controls[owners],stations)
        velocity=evaluate_controls(5*np.diff(controls[owners],axis=1),stations)
        acceleration=evaluate_controls(20*np.diff(controls[owners],n=2,axis=1),stations)
        speed=np.linalg.norm(velocity,axis=1)
        refined=controls
        for _ in range(3):
            refined=split_controls(refined)
        _,_,curvature=curve_bounds(refined)
        curvature=np.nan_to_num(curvature,nan=1e6,posinf=1e6)
        corners=body_outlines(xy,np.arctan2(velocity[:,1],velocity[:,0]),VEHICLE['widthM']*.5,
                             VEHICLE['frontFromRearAxleM'],VEHICLE['rearOverhangM'])
        points=shapely.points(corners.reshape(-1,2))
        distances=shapely.distance(points,boundary)
        distances=np.where(shapely.covers(surface,points),distances,-distances).reshape(-1,17,4)
        floor=4*np.maximum(.075-distances,0).reshape(segments,68)
        bends=4*np.maximum(curvature.reshape(segments,8)*VEHICLE['minimumRearAxleRadiusM']-.96,0)
        moving=np.maximum(.1-speed.reshape(segments,17)/step,0)
        return np.r_[np.c_[floor,bends,moving].ravel(),.005*(values-initial)]
    pattern=lil_matrix((93*segments+len(initial),len(initial)),dtype=int)
    for i in range(segments):
        for node in (i,i+1):
            if 0<node<count-1:
                pattern[i*93:(i+1)*93,(node-1)*6:node*6]=1
        if i==0:
            pattern[:93,-2]=1
        if i==segments-1:
            pattern[i*93:(i+1)*93,-1]=1
    for i in range(len(initial)):
        pattern[93*segments+i,i]=1
    lower=np.full(len(initial),-np.inf)
    lower[-2:]=step*.1
    fitted=least_squares(residual,initial,jac_sparsity=pattern.tocsr(),bounds=(lower,np.inf),
                         max_nfev=maximum_evaluations,ftol=1e-5,xtol=1e-6,gtol=1e-5)
    controls=decode(fitted.x)
    path,note=checked_curve(controls,surface,error)
    if path is not None and not same_island_winding(path,route,surface):
        path,note=None,dict(reason='source_winding_changed')
    return path,dict(note,nodes=count,evaluations=fitted.nfev,residual_calls=calls,
                     cost=fitted.cost,total_ms=(time.perf_counter()-started)*1000),controls


def run():
    results=[]
    for name,module,a,b in [('Wien',4524,0,1),('ZuerichHauptbahnhof',8207,0,1),('ZuerichHauptbahnhof',6958,0,2)]:
        network=DrivingNetwork(ROOT,name,VEHICLE)
        reference=next(p for p in REFERENCE['places'] if p['place']==name)
        network.frames={frame_key(f):{k:np.array(f[k]) for k in ('position','heading')} for f in reference['frames']}
        _,view=network.record(module)
        shape,surface,poses,_,_=view
        routes,_=directed_module_routes(network.graph,poses,shape,network.tree)
        for spacing in (8.,12.,4.):
            path,note,controls=optimize(poses[a],poses[b],routes[a,b],surface,network.curbs['arc_error_m'],spacing=spacing)
            row=dict(place=name,module=module,entry=a,exit=b,spacing_m=spacing,accepted=path is not None,**note)
            np.save(OUTPUT/f'{name}-{module}-controls-{int(spacing)}.npy',controls)
            results.append(row)
            print(json.dumps(row),flush=True)
            if path is not None:
                break
    (OUTPUT/'optimized.json').write_text(json.dumps(results,indent=2)+'\n')


if __name__=='__main__':
    run()
