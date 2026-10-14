"""Corruption, finite input, capacity and source immutability checks for the collar."""

import numpy as np
import shapely

from hip_exterior_mesh_probe import emit
from hip_exterior_surface_probe import prepare_collar
from outshine_ref.buildings.modelling import Shape,prepare_hip
from verify_hip_exterior_mesh import emitted_uv
from verify_modelling_roof_hip import measure
from verify_modelling_roof_shed import source


def rejects(call,error_type=ValueError):
    try:call()
    except error_type:return
    raise AssertionError('Expected rejection was not observed')


if __name__=='__main__':
    polygon=shapely.box(0,0,12,5);original=source(polygon)
    before={k:v.copy() for k,v in original.emit().items() if isinstance(v,np.ndarray)}
    proof=emit(original,polygon,method='collar');mesh=proof.shape.emit()
    emitted_uv(original,proof.shape);measure(mesh)
    for k,v in before.items():np.testing.assert_array_equal(original.emit()[k],v)
    wrong=dict(mesh);wrong['uv7']=mesh['uv7'].copy();wrong['uv7'][mesh['faces'][0]]+=[.25,.5]
    rejects(lambda:emitted_uv(original,Shape.from_mesh(wrong)))
    wrong=dict(mesh);wrong['faces']=mesh['faces'][1:]
    rejects(lambda:measure(wrong),AssertionError)
    plan=prepare_hip(polygon)
    for value in [0.,-1.,float('nan'),float('inf')]:
        rejects(lambda:prepare_collar(plan,overhang=value))
    for capacity in [0,-1,True,1]:
        rejects(lambda:prepare_collar(plan,max_pairs=capacity))
    rejects(lambda:emit(original,polygon,method='invalid'))
    rejects(lambda:emit(original,polygon,shell_depth_m=0.))
    rejects(lambda:emit(original.translate('rel','world',0.,2.,0.),polygon,method='collar'))
    print('PASS: UV7 corruption, missing triangle, finite inputs, capacities, unsupported source and immutability')
