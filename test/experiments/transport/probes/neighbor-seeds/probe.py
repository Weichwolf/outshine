import hashlib,json,sys,time
from pathlib import Path

import numpy as np
import shapely

sys.path.insert(0,str(Path('test/experiments/transport').resolve()))
from transport_driving_network import DrivingNetwork
from transport_directed_routes import directed_module_routes
from transport_hermite_route_fitting import fitted_route_candidates
from transport_junction_surfaces import motor_surface,same_island_winding
from transport_shared_frames import frame_key
from transport_surface_clearance import SurfaceClearance

class WholeBodyClearance:
    def __init__(self,surface): self.field=SurfaceClearance(surface)
    def __getattr__(self,name): return getattr(self.field,name)
    def shortfall(self,points,margin):
        xy=shapely.get_coordinates(points).reshape(-1,4,2)
        delta=np.roll(xy,-1,axis=1)-xy
        samples=xy[:,:,None,:]+delta[:,:,None,:]*np.array([0.,.25,.5,.75])[None,None,:,None]
        distance=self.field.shortfall(shapely.points(samples.reshape(-1,2)),margin).reshape(-1,4,4).max(axis=2)
        centre=self.field.shortfall(shapely.points(xy.mean(axis=1)),margin)
        return np.maximum(distance,centre[:,None]).ravel()

root=Path('build/experiments/transport')
reference=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
vehicle=json.loads(Path('test/experiments/transport/transport_network_recipes.json').read_text())['vehicleProfiles']['compact']
contact=lambda p:(p['section'],p['edge'],p['station'])
results=[]
for name,modules in [('Wien',[1383,3765]),('ZuerichHauptbahnhof',[102,1453]),('BaselBadischerBahnhof',[736,737,738])]:
    report=json.loads((root/'window-1000'/f'{name}-routes.json').read_text())
    original=next(p for p in reference['places'] if p['place']==name)
    network=DrivingNetwork(root,name,vehicle)
    network.frames={frame_key(f):{k:np.array(f[k]) for k in ('position','heading')} for f in original['frames']}
    stored={r['module']:r['paths'] for r in report['paths']}
    for row in report['modules']:
        module=row['module']
        if module not in modules: continue
        shape=network.shape(module)
        poses,_=network.poses(module,{})
        surface=motor_surface(shape,network.graph,network.tree,network.mesh,vehicle,
                              network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
        routes,_=directed_module_routes(network.graph,poses,shape,network.tree)
        accepted={(contact(poses[t['entry']]),contact(poses[t['exit']])):stored[module][i]
                  for i,t in enumerate(row['turns']) if t['accepted']}
        for turn in row['turns']:
            if turn['accepted']: continue
            a,b=turn['entry'],turn['exit']
            source=routes[a,b]
            other=accepted.get((contact(poses[b]),contact(poses[a])))
            candidates=[('source',source,None),('source_full_body',source,WholeBodyClearance(surface))]
            if other is not None:
                xy=np.array(other)[::-1].copy()
                distances=np.r_[0,np.cumsum(np.linalg.norm(np.diff(xy,axis=0),axis=1))]
                weight=distances/distances[-1]
                xy+=(1-weight)[:,None]*(poses[a]['position']-xy[0])+weight[:,None]*(poses[b]['position']-xy[-1])
                seed=shapely.LineString(xy)
                if same_island_winding(seed,source,surface): candidates.append(('reversed_neighbor',seed,None))
            for recipe,initial,field in candidates:
                started=time.perf_counter()
                path,note=fitted_route_candidates(poses[a],poses[b],initial,surface,vehicle,
                                                  network.curbs['arc_error_m'],maximum_calls=1024,clearance=field)
                valid=path is not None and same_island_winding(path,source,surface)
                result=dict(place=name,module=module,entry=a,exit=b,recipe=recipe,accepted=valid,
                            solve_ms=(time.perf_counter()-started)*1000,**note)
                if valid: result['path']=shapely.get_coordinates(path).tolist()
                results.append(result)
                print(json.dumps({k:v for k,v in result.items() if k not in ('path','fit_trials','vehicle_sweep')}),flush=True)
out=Path('build/experiments/transport/neighbor-seeds/body-results.json')
out.write_text(json.dumps(dict(results=results,implementation_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in [Path(__file__),Path('test/experiments/transport/transport_hermite_route_fitting.py')]}),indent=2)+'\n')
