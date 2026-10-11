from pathlib import Path
import cProfile
import io
import json
import pstats
import sys

import numpy as np

sys.path.insert(0,'test/experiments/infrastructure')
from infrastructure_driving_network import DrivingNetwork
from infrastructure_directed_routes import directed_module_routes
from infrastructure_shared_frames import frame_key
from infrastructure_hermite_route_fitting import fitted_route_candidates

root=Path('build/experiments/infrastructure')
vehicle=json.loads(Path('test/experiments/infrastructure/infrastructure_network_recipes.json').read_text())['vehicleProfiles']['compact']
reference=json.loads((root/'hermite-final/hermite-routes.json').read_text())
source=next(p for p in reference['places'] if p['place']=='BaselBadischerBahnhof')
network=DrivingNetwork(root,source['place'],vehicle)
network.frames={frame_key(f):{k:np.array(f[k]) for k in ('position','heading')} for f in source['frames']}
_,view=network.record(721)
shape,surface,poses,_,_=view
routes,_=directed_module_routes(network.graph,poses,shape,network.tree)
profile=cProfile.Profile()
profile.enable()
path,note=fitted_route_candidates(poses[3],poses[37],routes[3,37],surface,vehicle,network.curbs['arc_error_m'])
profile.disable()
profile.dump_stats(root/'distance-costs/fit-profile.dat')
print(json.dumps(dict(accepted=path is not None,**note)),flush=True)
out=io.StringIO()
pstats.Stats(profile,stream=out).strip_dirs().sort_stats('cumtime').print_stats(18)
print(out.getvalue())
