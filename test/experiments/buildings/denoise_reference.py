"""Run the external OIDN CPU filter on raw renderer buffers, preserving every input."""

import hashlib
from pathlib import Path
import subprocess
import time

import mitsuba as mi
import numpy as np


OIDN=Path.home()/'Downloads/oidn-2.5.1.x86_64.linux/bin/oidnDenoise'


def denoise(colour,albedo,normal,stem,log):
    stem=Path(stem);arrays=[np.asarray(value,dtype=np.float32) for value in (colour,albedo,normal)]
    if any(value.shape!=arrays[0].shape or value.ndim!=3 or value.shape[2]!=3 or not np.isfinite(value).all() for value in arrays):
        raise ValueError('OIDN inputs require matching finite RGB buffers')
    paths=[]
    for name,value in zip(('raw','albedo','normal'),arrays,strict=True):
        path=stem.with_name(stem.name+'-'+name+'.pfm');mi.Bitmap(np.ascontiguousarray(value)).write(str(path));paths.append(path)
    output=stem.with_name(stem.name+'-denoised.pfm');start=time.perf_counter()
    with Path(log).open('w') as stream:
        result=subprocess.run([str(OIDN),'--device','cpu','--threads','2','--quality','high','--hdr',str(paths[0]),
            '--alb',str(paths[1]),'--nrm',str(paths[2]),'--output',str(output),'-n','1'],stdout=stream,stderr=subprocess.STDOUT,timeout=120)
    if result.returncode:raise RuntimeError('OIDN CPU filter failed; inspect '+str(log))
    pixels=np.asarray(mi.Bitmap(str(output))).copy();seconds=time.perf_counter()-start
    if pixels.shape!=arrays[0].shape or not np.isfinite(pixels).all():raise ValueError('OIDN output is incomplete or non-finite')
    return pixels,dict(renderer='Open Image Denoise 2.5.1',backend='CPU',threads=2,quality='high',seconds=seconds,
        binary_path=str(OIDN),binary_sha256=hashlib.sha256(OIDN.read_bytes()).hexdigest(),input_files=[str(path) for path in paths],output_file=str(output),
        limits='Spatial renderer-buffer denoising; raw radiance/albedo/normals preserved; temporal stability and error against high-sample references unproved')
