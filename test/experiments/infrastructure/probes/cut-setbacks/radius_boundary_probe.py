import json
import sys
import time
from pathlib import Path

import numpy as np
import shapely

sys.path.insert(0,str(Path('test/experiments/infrastructure').resolve()))
from infrastructure_cut_setback_experiment import prepared_network,digest
from infrastructure_junction_surfaces import motor_surface,same_island_winding
from infrastructure_junction_turns import tangent_intersection
from infrastructure_directed_routes import directed_module_routes
from infrastructure_clothoid_turns import fit_symmetric_turns,sample_symmetric_turns
from infrastructure_clothoids import fit_clothoids,sample_clothoids
from infrastructure_vehicle_sweep import turn_body_check

root = Path('build/experiments/infrastructure')
name = 'BaselBadischerBahnhof'
base = json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames = json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
n = prepared_network(root,base,next(p['frames'] for p in frames['places'] if p['place']==name))
shape = n.shape(1109)
poses,_ = n.poses(1109,{})
floor = motor_surface(shape,n.graph,n.tree,n.mesh,n.vehicle,n.curbs['corner_radius_m'],n.curbs['arc_error_m'])
routes,_ = directed_module_routes(n.graph,poses,shape,n.tree)
results = []
for a,b in ((50,55),(56,49)):
    entry,exit = poses[a],poses[b]
    node,reason = tangent_intersection(entry,exit)
    assert reason is None
    h0,h1 = (np.array([np.arctan2(p['heading'][1],p['heading'][0])]) for p in (entry,exit))
    available = min(np.linalg.norm(node-entry['position']),np.linalg.norm(exit['position']-node))
    for profile,transition in [('G1',None)]+[('G2',t) for t in (.5,.25,.125,.0625,.03125,.015625)]:
        started = time.perf_counter()
        if profile=='G2':
            unit = fit_symmetric_turns(node,np.array([1.]),h0,h1,transition_fraction=transition)
            trim = float(unit['maximum_curvature_per_m'][0])*n.vehicle['minimumRearAxleRadiusM']
            fit = fit_symmetric_turns(node,np.array([trim]),h0,h1,transition_fraction=transition)
            sample = sample_symmetric_turns
        else:
            unit = fit_clothoids((node-entry['heading'])[None,:],(node+exit['heading'])[None,:],h0,h1)
            trim = float(unit['maximum_curvature_per_m'][0])*n.vehicle['minimumRearAxleRadiusM']
            fit = fit_clothoids((node-trim*entry['heading'])[None,:],(node+trim*exit['heading'])[None,:],h0,h1)
            sample = sample_clothoids
        row = dict(entry=a,exit=b,profile=profile,transition=transition,trim_m=trim,available_m=available)
        if trim<=available and fit['valid'][0]:
            body,_ = turn_body_check(floor,fit,entry['position'],exit['position'],profile,n.vehicle,.025)
            xy = sample(fit,np.linspace(0,1,129))[0][0]
            points = np.vstack((entry['position'],xy,exit['position']))
            points = points[np.r_[True,np.linalg.norm(np.diff(points,axis=0),axis=1)>1e-10]]
            path = shapely.LineString(points)
            winding = same_island_winding(path,routes[a,b],floor)
            row.update(body=body,winding=winding,accepted=body['fits_surface'] and winding,
                       radius_m=1/float(fit['maximum_curvature_per_m'][0]))
            if row['accepted']:
                row['path'] = points.tolist()
        else:
            row.update(accepted=False,reason='trim_outside_available')
        row['elapsed_ms'] = (time.perf_counter()-started)*1000
        results.append(row)
        print(json.dumps({k:v for k,v in row.items() if k not in ('path','body')}|{'body':row.get('body')}),flush=True)
Path('build/experiments/infrastructure/cut-setbacks/radius-boundary-results.json').write_text(json.dumps(dict(results=results,
    prototype_sha256=digest(Path(__file__)),input_sha256=base['input_sha256']),indent=2)+'\n')
