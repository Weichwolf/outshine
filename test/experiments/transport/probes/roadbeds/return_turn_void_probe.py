"""Identify the topological conflict between individually valid return turns."""

import json
from pathlib import Path
import runpy
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import transport_coupled_roadbed_experiment as coupled
from transport_turn_roadbeds import compose_roadbeds,bounded_voids,coordinate_roundoff

original=coupled.coupled_addition_rejection


def inspect(floor,shape,supports,*args):
    reason=original(floor,shape,supports,*args)
    if reason=='combined_void_topology':
        base,core,planned,physical,_=compose_roadbeds(floor,shape,supports)
        data={}
        for name,before,after in (('public',base,planned),('owned',core,physical)):
            old,new=bounded_voids(before),bounded_voids(after)
            data[name]=dict(before_parts=len(shapely.get_parts(before)),after_parts=len(shapely.get_parts(after)),
                           old_holes=shapely.area(old).tolist(),new_holes=shapely.area(new).tolist(),roundoff_m=coordinate_roundoff(before))
            details=[]
            for hole in new:
                if hole.area>1e-8:continue
                origin=shapely.get_coordinates(hole)[0]
                rectangle=shapely.minimum_rotated_rectangle(shapely.transform(hole,lambda xy:xy-origin))
                width=float(np.linalg.norm(np.diff(shapely.get_coordinates(rectangle),axis=0),axis=1).min())
                details.append(dict(width_m=width,overlaps=[hole.intersection(h).area for h in old]))
            data[name]['small_holes']=details
            if details:
                path=Path('build/experiments/transport/roadbeds/return-turn-void-input.json')
                path.write_text(json.dumps(dict(before=before.wkb.hex(),after=after.wkb.hex(),precision=shapely.get_precision(after)))+'\n')
        print(json.dumps(dict(rejection=reason,topology=data)),flush=True)
    return reason


coupled.coupled_addition_rejection=inspect
runpy.run_path(str(Path(__file__).with_name('return_turn_coupled_probe.py')),run_name='__main__')
