"""Locate publication precision defects without accepting rejected products."""

import argparse
import json
from pathlib import Path
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_coupled_roadbed_experiment as coupled

original=coupled.published_roadbed_surface


def inspect(mesh,base,core,planned,precision):
    vertices=mesh['vertices'];used=vertices[np.unique(mesh['indices'])]
    faces=shapely.polygons(vertices[mesh['indices']])
    actual=shapely.union_all(shapely.set_precision(faces,precision))
    complete=actual.intersection(planned).union(base.difference(core))
    parts=shapely.get_parts(complete)
    surface=shapely.union_all(parts[np.isin(shapely.get_type_id(parts),[3,6])])
    print(json.dumps(dict(quantum=precision,all_rounding_m=float(np.abs(vertices-np.rint(vertices/precision)*precision).max()),
        owned_rounding_m=float(np.abs(used-np.rint(used/precision)*precision).max()),
        component_types=shapely.get_type_id(parts).tolist(),area_difference=complete.symmetric_difference(surface).area)),flush=True)
    return original(mesh,base,core,planned,precision)


parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--module',type=int,required=True)
parser.add_argument('--pair',type=int,nargs=2,required=True)
args=parser.parse_args()
coupled.published_roadbed_surface=inspect
coupled.real_case(Path('build/experiments/transport'),'BaselBadischerBahnhof',args.module,args.pair,4.,
    Path('build/experiments/transport/roadbeds/precision-diagnostic')/str(args.module))
