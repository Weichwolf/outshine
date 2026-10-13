"""Call the pinned exported polygon helper in a bounded child; not a complete CGA offset oracle."""

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import numpy as np
import shapely

from esri_ghidra import CORE


DIGEST='850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45'
SYMBOL='_ZN11CoreToolbox10PolygonOps13computeOffsetEfPKfm'


def worker(request):
    import resource
    resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_CPU,(8,8))
    if hashlib.sha256(CORE.read_bytes()).hexdigest()!=DIGEST:raise ValueError('Offset helper ABI requires the pinned SDK binary')
    xy=np.asarray(request['xy'],dtype=np.float32);distance=float(request['distance'])
    if xy.ndim!=2 or xy.shape[1]!=2 or not 3<=len(xy)<=256 or not np.isfinite(xy).all():raise ValueError('Invalid oracle ring')
    polygon=shapely.Polygon(xy)
    if not polygon.is_valid or polygon.is_empty or not 0<distance<1e6:raise ValueError('Oracle requires a simple ring and positive inward distance')
    xy=np.asarray(shapely.orient_polygons(polygon).exterior.coords,dtype=np.float32)[:-1]
    vertices=np.ascontiguousarray(np.c_[xy[:,0],np.zeros(len(xy)),xy[:,1]],dtype=np.float32)
    library=ctypes.CDLL(str(CORE));function=getattr(library,SYMBOL)
    function.argtypes=[ctypes.c_float,ctypes.POINTER(ctypes.c_float),ctypes.c_size_t]
    function.restype=ctypes.POINTER(ctypes.c_void_p)
    result=function(distance,vertices.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),len(vertices))
    start,end,capacity=(result[index] or 0 for index in range(3));length=end-start
    if not 0<=length<=256*12*16 or length%12 or not end<=capacity:raise ValueError('SDK returned an invalid vector range')
    values=np.ctypeslib.as_array((ctypes.c_float*(length//4)).from_address(start)).copy().reshape(-1,3) if length else np.empty((0,3))
    if not np.isfinite(values).all():raise ValueError('SDK offset returned nonfinite vertices')
    return dict(vertices=values.tolist(),binary_sha256=DIGEST,symbol=SYMBOL,
        scope='XZ planar helper, positive input means inward; emits only the largest component; child exit releases allocations')


def reference(polygon,distance):
    if polygon.interiors:raise ValueError('Exported helper has no hole-ring argument')
    request=dict(xy=list(polygon.exterior.coords)[:-1],distance=distance)
    env=os.environ.copy();env['LD_LIBRARY_PATH']=str(CORE.parent)+os.pathsep+env.get('LD_LIBRARY_PATH','')
    result=subprocess.run([sys.executable,str(Path(__file__).resolve()),'--worker'],input=json.dumps(request),
        text=True,capture_output=True,env=env,timeout=10)
    if result.returncode:raise RuntimeError(f'Isolated SDK helper failed ({result.returncode}): '+result.stderr[-500:])
    return json.loads(result.stdout)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--worker',action='store_true');args=parser.parse_args()
    if args.worker:print(json.dumps(worker(json.load(sys.stdin))))
    else:parser.error('Use reference() from an explicit oracle experiment')
