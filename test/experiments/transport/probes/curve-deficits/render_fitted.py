from pathlib import Path
import hashlib
import json
import sys

import numpy as np

sys.path.insert(0,'test/experiments/transport')
from transport_driving_network import DrivingNetwork
from transport_shared_frames import frame_key,accepted_routes
from transport_hermite_experiment import fit_module
from transport_junction_turn_experiment import render

root=Path('build/experiments/transport')
report=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
vehicle=json.loads(Path('test/experiments/transport/transport_network_recipes.json').read_text())['vehicleProfiles']['compact']
for name,digest in report['implementation_sha256'].items():
    assert hashlib.sha256((Path('test/experiments/transport')/name).read_bytes()).hexdigest()==digest,name
before,after=[],[]
for name,module in [('Wien',1100),('ZuerichHauptbahnhof',8207),('ZuerichHauptbahnhof',6958)]:
    source=next(p for p in report['places'] if p['place']==name)
    for path,digest in source['input_sha256'].items():
        assert hashlib.sha256((root/path).read_bytes()).hexdigest()==digest,path
    network=DrivingNetwork(root,name,vehicle)
    network.frames={frame_key(f):{k:np.array(f[k]) for k in ('position','heading')} for f in source['frames']}
    original=network.record(module)
    fitted,trials=fit_module(network,module,1024)
    expected=next(m for m in source['modules'] if m['module']==module)
    assert accepted_routes(fitted[0])==accepted_routes(expected)
    assert original[1][0] is fitted[1][0] and original[1][1] is fitted[1][1]
    assert original[1][2] is fitted[1][2]
    before.append((name+' before',module,*original))
    after.append((name+' after',module,*fitted))
    print(json.dumps(dict(place=name,module=module,before=original[0]['accepted'],after=fitted[0]['accepted'],
                         requests=fitted[0]['connections'],unchanged_surface_and_ports=True)),flush=True)
render(Path('build/shots/experiments/transport/fitted-corridor-routes.png'),before+after,
       'C2 route fitting: same road surfaces and shared ports; full body and radius checked')
