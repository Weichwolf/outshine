"""Bounded pinned SDK helper: planar skeleton facets, without CGA roof-height semantics."""

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
from esri_offset_oracle import DIGEST

SYMBOL = '_ZN11CoreToolbox10PolygonOps15computeSkeletonEPKfm'


def worker(request):
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_CPU, (8, 8))
    if hashlib.sha256(CORE.read_bytes()).hexdigest() != DIGEST:
        raise ValueError('Skeleton helper requires the pinned SDK binary')
    xy = np.asarray(request['xy'], dtype=np.float32)
    if xy.ndim != 2 or xy.shape[1] != 2 or not 3 <= len(xy) <= 256 or not np.isfinite(xy).all():
        raise ValueError('Invalid oracle outline')
    polygon = shapely.Polygon(xy)
    if not polygon.is_valid or polygon.area <= 1e-8:
        raise ValueError('Skeleton oracle requires a valid simple outline')
    oriented = np.asarray(shapely.orient_polygons(polygon, exterior_cw=True).exterior.coords)[:-1]
    vertices = np.ascontiguousarray(np.c_[oriented[:, 0], np.zeros(len(oriented)), oriented[:, 1]], dtype=np.float32)
    library = ctypes.CDLL(str(CORE))
    function = getattr(library, SYMBOL)
    function.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.c_size_t]
    function.restype = ctypes.POINTER(ctypes.c_void_p)
    result = function(vertices.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), len(vertices))
    if not result:
        raise ValueError('SDK returned no vector')
    start, end, capacity = (result[index] or 0 for index in range(3))
    size = end-start
    if not start or not 0 < size <= 4*256*64 or size % 4 or not start <= end <= capacity:
        raise ValueError('SDK returned an invalid float vector range')
    values = np.ctypeslib.as_array((ctypes.c_float*(size//4)).from_address(start)).copy()
    separators = np.flatnonzero(np.isposinf(values))
    if not len(separators) or separators[-1] != len(values)-1:
        raise ValueError('SDK facet terminator is missing')
    facets, first = [], 0
    for last in separators:
        points = values[first:last]
        if len(points) < 9 or len(points) % 3 or not np.isfinite(points).all():
            raise ValueError('SDK returned invalid facet coordinates')
        points = points.reshape(-1, 3)
        if np.any(points[:, 1] != 0):
            raise ValueError('Planar helper unexpectedly changed the input height')
        facet = shapely.Polygon(points[:, [0, 2]])
        if not facet.is_valid or facet.area <= 1e-8:
            raise ValueError('SDK returned an invalid facet polygon')
        facets.append(points[:, [0, 2]].tolist()); first = int(last)+1
    return dict(facets=facets, input_xy=xy.tolist(), binary_sha256=DIGEST, symbol=SYMBOL,
                scope='Float32 XZ outline; planar facet partition only, no holes/roof heights/UVs/overhang/even/trim. Child exit releases allocations.')


def reference(polygon):
    if polygon.geom_type != 'Polygon' or polygon.interiors:
        raise ValueError('Exported skeleton helper accepts one outline without holes')
    request = dict(xy=list(polygon.exterior.coords)[:-1])
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = str(CORE.parent)+os.pathsep+env.get('LD_LIBRARY_PATH', '')
    env['OPENBLAS_NUM_THREADS'] = '1'
    result = subprocess.run([sys.executable, str(Path(__file__).resolve()), '--worker'], input=json.dumps(request),
                            text=True, capture_output=True, env=env, timeout=10)
    if result.returncode:
        raise RuntimeError(f'Isolated SDK skeleton failed ({result.returncode}): '+result.stderr[-500:])
    return json.loads(result.stdout)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker', action='store_true')
    args = parser.parse_args()
    if not args.worker:
        parser.error('Use reference() from an explicit oracle experiment')
    print(json.dumps(worker(json.load(sys.stdin))))
