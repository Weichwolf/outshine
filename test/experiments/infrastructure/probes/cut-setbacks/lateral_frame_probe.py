import copy
import json
import sys
import time
from pathlib import Path

import numpy as np
import shapely

sys.path.insert(0,str(Path('test/experiments/infrastructure').resolve()))
from infrastructure_cut_setback_experiment import prepared_network,digest
from infrastructure_hermite_experiment import fit_module
from infrastructure_shared_frames import frame_key,ranked_pairs,preserves_routes
from infrastructure_junction_surfaces import motor_surface,same_island_winding
from infrastructure_junction_turns import connect_lanes,tangent_intersection
from infrastructure_directed_routes import directed_module_routes
from infrastructure_clothoid_turns import fit_symmetric_turns,sample_symmetric_turns
from infrastructure_vehicle_sweep import turn_body_check


def minimum_radius_curve(entry,exit,floor,vehicle):
    node,reason = tangent_intersection(entry,exit)
    if reason is not None:
        return None,{}
    h0,h1 = (np.array([np.arctan2(p['heading'][1],p['heading'][0])]) for p in (entry,exit))
    available = min(np.linalg.norm(node-entry['position']),np.linalg.norm(exit['position']-node))
    for transition in (.25,.125,.0625):
        unit = fit_symmetric_turns(node,np.array([1.]),h0,h1,transition_fraction=transition)
        trim = float(unit['maximum_curvature_per_m'][0])*vehicle['minimumRearAxleRadiusM']
        if trim<=0 or trim>available:
            continue
        fit = fit_symmetric_turns(node,np.array([trim]),h0,h1,transition_fraction=transition)
        body,_ = turn_body_check(floor,fit,entry['position'],exit['position'],'G2',vehicle,.025)
        if body['fits_surface']:
            points = np.vstack((entry['position'],sample_symmetric_turns(fit,np.linspace(0,1,129))[0][0],exit['position']))
            points = points[np.r_[True,np.linalg.norm(np.diff(points,axis=0),axis=1)>1e-10]]
            return shapely.LineString(points),dict(profile='minimum-radius',transition_fraction=transition,vehicle_sweep=body)
    return None,{}

root = Path('build/experiments/infrastructure')
base = json.loads((root/'window-1000-body/BaselBadischerBahnhof-routes.json').read_text())
frames = json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
n = prepared_network(root,base,next(p['frames'] for p in frames['places'] if p['place']=='BaselBadischerBahnhof'))
module = 1109
poses,_ = n.poses(module,{})
entry,exit = poses[50],poses[55]
floor = motor_surface(n.shape(module),n.graph,n.tree,n.mesh,n.vehicle,n.curbs['corner_radius_m'],.025)
routes,_ = directed_module_routes(n.graph,poses,n.shape(module),n.tree)
affected = set.union(*(n.owners[frame_key(p)] for p in (entry,exit)))
before = {r['module']:r for r in base['modules'] if r['module'] in affected}
before.update((m,fit_module(n,m,1024)[0][0]) for m in affected if m not in before)
choices = [n.pose_options(p) for p in (entry,exit)]
print(json.dumps(dict(domains=list(map(len,choices)),pairs=np.prod(list(map(len,choices))).item())),flush=True)
started = time.perf_counter()
trials,winner = [],None
for index,pair in enumerate(ranked_pairs(choices)):
    if index>=1024:
        break
    a,b = [dict(p,**c[0]) for p,c in zip((entry,exit),pair)]
    path,note = connect_lanes(a,b,floor,n.vehicle,.025)
    if path is None:
        path,note = minimum_radius_curve(a,b,floor,n.vehicle)
    if path is None or not same_island_winding(path,routes[50,55],floor):
        continue
    candidate = copy.copy(n)
    candidate.frames = dict(n.frames)
    candidate.frames.update({frame_key(p):{k:p[k] for k in ('position','heading')} for p in (a,b)})
    candidate.records = {}
    after = {m:fit_module(candidate,m,1024)[0] for m in sorted(affected)}
    accepted = preserves_routes(before,{m:r[0] for m,r in after.items()}) and any(
        t['entry']==50 and t['exit']==55 and t['accepted'] for t in after[module][0]['turns'])
    row = dict(rank=index+1,accepted=accepted,cost_m=sum(p[1] for p in pair),
        before={m:[before[m]['accepted'],before[m]['connections']] for m in sorted(affected)},
        after={m:[after[m][0]['accepted'],after[m][0]['connections']] for m in sorted(affected)},
        changes=[{k:p[k].tolist() for k in ('position','heading')} for p in (a,b)])
    trials.append(row)
    print(json.dumps(row),flush=True)
    if accepted:
        winner = row
        break
result = dict(winner=winner,trials=trials,pairs_tried=index+1,elapsed_ms=(time.perf_counter()-started)*1000,
              prototype_sha256=digest(Path(__file__)),input_sha256=base['input_sha256'])
Path('build/experiments/infrastructure/cut-setbacks/lateral-frame-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ('trials','winner','input_sha256')}),flush=True)
