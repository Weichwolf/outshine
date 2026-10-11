"""Reproduce one window module and retain publication inputs for geometric diagnosis."""

import argparse
import json
from pathlib import Path
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_roadbed_window_experiment as windows

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--place',required=True)
parser.add_argument('--module',type=int,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
original_propose=windows.propose_roadbeds
original_publish=windows.published_roadbed_surface


def propose(network,current,reserved,changed):
    current=dict(current,modules=[r for r in current['modules'] if r['module']==args.module])
    return original_propose(network,current,reserved,changed)


def publish(mesh,base,core,planned,precision):
    np.savez_compressed(args.output/'mesh.npz',**mesh)
    data=dict(precision=precision,**{k:v.wkb.hex() for k,v in dict(base=base,core=core,planned=planned).items()})
    (args.output/'geometry.json').write_text(json.dumps(data)+'\n')
    result=original_publish(mesh,base,core,planned,precision)
    print(json.dumps(dict(surface_valid=bool(shapely.is_valid(result[0])),reason=shapely.is_valid_reason(result[0]),
                         components=len(shapely.get_parts(result[0])))),flush=True)
    return result


windows.propose_roadbeds=propose
windows.published_roadbed_surface=publish
root=Path('build/experiments/infrastructure')
frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
result=windows.solve_window(root,args.place,next(p['frames'] for p in frames['places'] if p['place']==args.place),set(),args.output,[])
print(json.dumps({k:result[k] for k in ('place','native_accepted','elapsed_ms')}))
