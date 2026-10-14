"""Isolated version-pinned SDK roof geometry kernel; not the Processor/CGA UV pipeline."""

import argparse
import ctypes as c
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
from esri_skeleton_oracle import SYMBOL

POLYGON_BYTES=160


class Vector(c.Structure):
    _fields_=[('start',c.c_void_p),('end',c.c_void_p),('capacity',c.c_void_p)]


class Small(c.Structure):
    _fields_=[('start',c.c_void_p),('size',c.c_size_t),('capacity',c.c_size_t),('inline',c.c_uint32*4)]


class String(c.Structure):
    _fields_=[('start',c.c_void_p),('size',c.c_size_t),('inline',c.c_char*16)]


def readable(address,size):
    if not address or size<=0:raise ValueError('Empty SDK output range')
    end=address+size;cursor=address
    for line in Path('/proc/self/maps').read_text().splitlines():
        fields=line.split();a,b=(int(v,16) for v in fields[0].split('-'))
        if a<=cursor<b and fields[1].startswith('r'):
            cursor=min(b,end)
            if cursor==end:return
    raise ValueError('SDK range is not entirely mapped readable memory')


def vector(array):
    return Vector(array.ctypes.data,array.ctypes.data+array.nbytes,array.ctypes.data+array.nbytes)


def small(array):return Small(array.ctypes.data,len(array),max(4,len(array)))


def count(header,width,limit):
    start,end,capacity=header.start or 0,header.end or 0,header.capacity or 0
    if not start<=end<=capacity or (end-start)%width or not 0<end-start<=limit*width:
        raise ValueError('SDK vector violates its bounded output ABI')
    readable(start,end-start);return (end-start)//width


def loader():
    if sys.byteorder!='little' or c.sizeof(c.c_void_p)!=8 or hashlib.sha256(CORE.read_bytes()).hexdigest()!=DIGEST:
        raise ValueError('Roof oracle requires the pinned little-endian 64-bit SDK')
    library=c.CDLL(str(CORE));address=c.cast(getattr(library,SYMBOL),c.c_void_p).value
    rows=subprocess.check_output(['nm','-D',str(CORE)],text=True).splitlines()
    offsets=[int(row.split()[0],16) for row in rows if row.split()[-1:]==[SYMBOL]]
    if len(offsets)!=1:raise ValueError('Expected one version-pinned exported anchor')
    base=address-offsets[0]
    def function(offset,args,restype=None):return c.CFUNCTYPE(restype,*args)(base+offset)
    return library,function


def build(function,rings):
    vertices=np.ascontiguousarray(np.c_[np.concatenate(rings),np.zeros(sum(map(len,rings)))])
    normals=np.empty((0,3),dtype=np.float64);polys=c.create_string_buffer(POLYGON_BYTES*len(rings))
    offset=0
    for row,ring in enumerate(rings):
        ids=np.arange(offset,offset+len(ring),dtype=np.uint32);header=small(ids)
        normal=np.array([0.,0.,1. if row==0 else -1.]);offset+=len(ring)
        function(0x410040,[c.c_void_p]*3)(c.addressof(polys)+row*POLYGON_BYTES,c.byref(header),normal.ctypes.data)
    name=String();name.start=c.addressof(name)+16
    vv,nn=vector(vertices),vector(normals)
    pp=Vector(c.addressof(polys),c.addressof(polys)+len(polys),c.addressof(polys)+len(polys))
    mesh=c.create_string_buffer(4096)
    function(0x48e200,[c.c_void_p]*5)(c.addressof(mesh),c.byref(name),c.byref(vv),c.byref(nn),c.byref(pp))
    relations=np.asarray([0,*range(1,len(rings)),2**32-1] if len(rings)>1 else [],dtype=np.uint32)
    rel=small(relations);holes=c.create_string_buffer(256)
    function(0x2e48b0,[c.c_void_p]*2)(c.addressof(holes),c.byref(rel))
    return mesh,holes


def output(roof):
    if not roof:raise ValueError('SDK rejected the roof geometry')
    readable(roof,424)
    vertices=Vector.from_address(roof+40);polygons=Vector.from_address(roof+112)
    size=count(vertices,24,4096);faces_count=count(polygons,POLYGON_BYTES,1024)
    points=np.ctypeslib.as_array((c.c_double*(size*3)).from_address(vertices.start)).copy().reshape(-1,3)
    if not np.isfinite(points).all():raise ValueError('SDK roof coordinates are nonfinite')
    faces=[]
    for row in range(faces_count):
        ring=Small.from_address(polygons.start+row*POLYGON_BYTES)
        if not 3<=ring.size<=1024 or ring.capacity<ring.size:raise ValueError('Invalid SDK polygon ring')
        readable(ring.start,ring.size*4)
        ids=np.ctypeslib.as_array((c.c_uint32*ring.size).from_address(ring.start)).copy()
        if np.any(ids>=size):raise ValueError('SDK polygon index is outside the output')
        faces.append(ids.tolist())
    return points,faces


def worker(request):
    import resource
    resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_CPU,(8,8))
    resource.setrlimit(resource.RLIMIT_AS,(2*2**30,2*2**30))
    polygon=shapely.Polygon(request['outer'],request.get('holes',[]))
    if not polygon.is_valid or polygon.is_empty or polygon.area<=1e-8:raise ValueError('Invalid SDK oracle domain')
    polygon=shapely.orient_polygons(polygon)
    rings=[np.asarray(r.coords,dtype=float)[:-1] for r in (polygon.exterior,*polygon.interiors)]
    if not 3<=sum(map(len,rings))<=256 or not all(np.isfinite(r).all() for r in rings):raise ValueError('Oracle input capacity exceeded')
    value=float(request['value']);overhang=float(request['overhang']);mode=request['mode'];even=request['even']
    if mode not in ('byAngle','byHeight') or value<=0 or not np.isfinite([value,overhang]).all() or type(even)is not bool:
        raise ValueError('Invalid oracle roof options')
    library,function=loader();mesh,holes=build(function,rings);origins=Vector()
    roof=function(0x5697b0,[c.c_void_p,c.c_void_p,c.c_uint,c.c_uint,c.c_double,c.c_bool,c.c_double,c.c_double,c.c_void_p],c.c_void_p)(
        c.addressof(mesh),c.addressof(holes),0,int(even),value,mode=='byHeight',overhang,0.,c.byref(origins))
    points,faces=output(roof)
    return dict(positions_m=points.tolist(),faces=faces,binary_sha256=DIGEST,
                scope='Pinned internal roof geometry kernel in planar XY/Z-up coordinates; no Processor bottom/tags/scope, source UV/material/trim or full CGA rule. Child exit releases all SDK allocations.')


def reference(polygon,value=3.,mode='byHeight',overhang=0.,even=False):
    request=dict(outer=list(polygon.exterior.coords)[:-1],holes=[list(r.coords)[:-1] for r in polygon.interiors],value=value,mode=mode,overhang=overhang,even=even)
    env=os.environ.copy();env['LD_LIBRARY_PATH']=str(CORE.parent)+os.pathsep+env.get('LD_LIBRARY_PATH','');env['OPENBLAS_NUM_THREADS']='1'
    child=subprocess.run([sys.executable,str(Path(__file__).resolve()),'--worker'],input=json.dumps(request),text=True,capture_output=True,env=env,timeout=10)
    if child.returncode:raise RuntimeError(f'Isolated roof oracle failed ({child.returncode}): '+child.stderr[-500:])
    return json.loads(child.stdout)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--worker',action='store_true');args=parser.parse_args()
    if not args.worker:parser.error('Use reference() from an explicit SDK comparison')
    print(json.dumps(worker(json.load(sys.stdin))))
