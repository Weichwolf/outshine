import json,sys,time
from pathlib import Path
import numpy as np
import shapely
sys.path.insert(0,'test/experiments/infrastructure')
from infrastructure_turn_roadbed_experiment import prepared_network,building_reservations,reservations,motor_surface,directed_module_routes
from infrastructure_cut_setbacks import restore_cut_plan,setback_network,CutSetbackRejected
from infrastructure_shared_frames import frame_key
from infrastructure_source_roadbeds import construct_route_roadbed

root=Path('build/experiments/infrastructure');name='ZuerichHauptbahnhof';module=102
before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
network=prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
cuts=json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text());restore_cut_plan(network,cuts['plan'])
shape=network.shape(module);poses,_=network.poses(module,{})
floor=motor_surface(shape,network.graph,network.tree,network.mesh,network.vehicle,network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
region=floor.envelope.buffer(network.vehicle['widthM'])
buildings,tiles=building_reservations(receipt,region)
reserved=buildings.union(reservations(network,region).difference(floor))
for old_pair in ((2,4),(5,3)):
    wanted=[frame_key(poses[i])[:3] for i in old_pair]
    sections={poses[i]['section'] for i in old_pair}
    for setback in (4.,8.,12.):
        start=time.perf_counter()
        try:
            trial,changes,affected=setback_network(network,module,sections,setback)
            trial_poses,_=trial.poses(module,{})
            keys=[frame_key(p)[:3] for p in trial_poses]
            pair=tuple(keys.index(k) for k in wanted)
            trial_floor=floor if trial.shape(module).bounds==shape.bounds else motor_surface(trial.shape(module),trial.graph,
                    trial.tree,trial.mesh,trial.vehicle,trial.curbs['corner_radius_m'],trial.curbs['arc_error_m'])
            routes,_=directed_module_routes(trial.graph,trial_poses,trial.shape(module),trial.tree)
            candidate,notes=construct_route_roadbed(trial_poses[pair[0]],trial_poses[pair[1]],trial_floor,reserved,
                                    routes[pair],trial.vehicle,trial.curbs['arc_error_m'])
            print(json.dumps(dict(pair=old_pair,setback_m=setback,accepted=candidate is not None,
                reason=notes[-1].get('reason'),affected=sorted(affected),elapsed_ms=(time.perf_counter()-start)*1000,
                added_area_m2=candidate['addition'].area if candidate else None)),flush=True)
        except CutSetbackRejected as error:
            print(json.dumps(dict(pair=old_pair,setback_m=setback,rejected=str(error))),flush=True)
