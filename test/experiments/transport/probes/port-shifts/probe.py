from itertools import product
from pathlib import Path
import json,time,numpy as np,shapely

from transport_driving_network import DrivingNetwork
from transport_junction_turn_experiment import solve_module
from transport_directed_routes import directed_module_routes
from transport_junction_turns import connect_lanes
from transport_junction_surfaces import same_island_winding
from transport_shared_frames import frame_key,preserves_routes,accepted_routes
from transport_vehicle_sweep import body_outlines

root=Path('build/experiments/transport')
reference=json.loads((root/'shared-frames/shared-frames.json').read_text())
vehicle=json.loads(Path('test/experiments/transport/transport_network_recipes.json').read_text())['vehicleProfiles']['compact']

def choices(network,pose):
    key=frame_key(pose)
    junction=next(k for k in network.owners[key] if k in network.junctions)
    port=next(p['line'] for p in network.junctions[junction] if p['section']==pose['section'])
    endpoints=shapely.get_coordinates(port)
    tangent=(endpoints[1]-endpoints[0])/port.length
    half=network.graph['half_widths'][pose['edge']]
    anchor=network.anchors[key]
    right=np.array([anchor[1],-anchor[0]])
    axis=np.array(network.graph['lines'][pose['edge']].interpolate(pose['station']).coords[0])
    both=np.all(network.graph['travel_public'][pose['edge']]&1!=0)
    own_lane=both and not pose['shared_lane']
    initial=np.arctan2(anchor[1],anchor[0])
    current=np.arctan2(pose['heading'][1],pose['heading'][0])
    surfaces=[network.record(k)[1][1] for k in network.owners[key]]
    positions=[pose['position']+tangent*step*half for step in np.linspace(-1,1,17)]
    angles=np.r_[current,initial+np.deg2rad(np.arange(-30,31,5))]
    error=network.curbs['arc_error_m']
    result=[]
    for position,angle in product(positions,angles):
        if port.distance(shapely.Point(position))>1e-7:continue
        heading=np.array([np.cos(angle),np.sin(angle)])
        if any(np.linalg.norm(position-p['position'])<1e-10 and np.linalg.norm(heading-p['heading'])<1e-10 for p,_ in result):continue
        body=shapely.Polygon(body_outlines(position[None,:],[angle],vehicle['widthM']*.5,
                            vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])[0])
        if not all(shapely.covers(s,body) and s.boundary.distance(body)>=error for s in surfaces):continue
        section=shapely.get_coordinates(shapely.intersection(body,port))
        lateral=np.einsum('ij,j->i',section-axis,right)
        if not len(lateral) or lateral.min()<(error if own_lane else -half+error) or lateral.max()>half-error:continue
        distance=np.linalg.norm(position-pose['position'])
        change=abs(np.arctan2(np.sin(angle-current),np.cos(angle-current)))
        result.append((dict(pose,position=position,heading=heading),float(distance+vehicle['minimumRearAxleRadiusM']*change)))
    return result

def solve_variant(network,module,overrides):
    poses,unused=network.poses(module,{key:dict(heading=p['heading']) for key,p in overrides.items()})
    poses=[dict(p,position=overrides.get(frame_key(p),p)['position']) for p in poses]
    return solve_module(module,network.shape(module),poses,unused,network.graph,network.tree,
                        network.mesh,vehicle,network.curbs,adjust_ports=False)

results=[]
for place in reference['places']:
    network=DrivingNetwork(root,place['place'],vehicle)
    network.frames={(f['section'],f['edge'],f['direction'],f['station']):{k:np.array(f[k]) for k in ('position','heading') if k in f} for f in place['frames']}
    for module in place['modules']:
        for turn in module['turns']:
            if turn['accepted']:continue
            began=time.perf_counter()
            current,view=network.record(module['module'])
            shape,surface,poses,_,_=view
            a,b=turn['entry'],turn['exit']
            entry,exit=poses[a],poses[b]
            affected=set.union(*(network.owners[frame_key(p)] for p in (entry,exit)))
            before={k:network.record(k)[0] for k in affected}
            source,_=directed_module_routes(network.graph,poses,shape,network.tree)
            options=[choices(network,p) for p in (entry,exit)]
            record=dict(place=place['place'],module=module['module'],entry=a,exit=b,
                        choices=[len(o) for o in options],tried=0,curve_fits=0,
                        neighbor_trials=0,rejected_loss=0,accepted=False)
            fixed=[[c for c in o if np.linalg.norm(c[0]['position']-p['position'])<1e-10] for o,p in zip(options,(entry,exit))]
            variants=[]
            for label,pool in [('headings',fixed),('positions',options)]:
                candidates=sorted(product(*pool),key=lambda pair:sum(c[1] for c in pair))
                trial=dict(method=label,available=len(candidates),tried=0,curve_fits=0,neighbor_trials=0,rejected_loss=0,accepted=False)
                for pair in candidates[:64]:
                    trial['tried']+=1
                    p,q=pair[0][0],pair[1][0]
                    path,_=connect_lanes(p,q,surface,vehicle,network.curbs['arc_error_m'])
                    if path is None or current['directed_island_winding'] and not same_island_winding(path,source[a,b],surface):continue
                    trial['curve_fits']+=1
                    overrides={frame_key(s):s for s in (p,q)}
                    after={k:solve_variant(network,k,overrides)[0] for k in affected}
                    trial['neighbor_trials']+=1
                    if not preserves_routes(before,after):
                        trial['rejected_loss']+=1
                        continue
                    if (a,b) not in accepted_routes(after[module['module']]):continue
                    trial.update(accepted=True,poses=[dict(position=s['position'].tolist(),heading=s['heading'].tolist()) for s in (p,q)],
                                  displacements_m=[float(np.linalg.norm(s['position']-original['position'])) for s,original in zip((p,q),(entry,exit))],
                                  affected_modules=sorted(affected),gained=sum(len(accepted_routes(after[k])-accepted_routes(before[k])) for k in affected))
                    break
                variants.append(trial)
            record['variants']=variants
            record['total_ms']=(time.perf_counter()-began)*1000
            results.append(record)
            print(json.dumps(record),flush=True)
(root/'port-shifts/probe.json').write_text(json.dumps(results,indent=2)+'\n')
