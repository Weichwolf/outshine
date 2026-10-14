"""Equal-profile construction comparison and real supplied special-structure dimensions."""

import argparse
from dataclasses import asdict
from collections import Counter
import json
import math
from pathlib import Path
import resource
import time

import numpy as np
import shapely
import trimesh

from evidence import revision
from special_forms import profile
from outshine_ref.buildings.structures import StructureDecision,StructureMetrics
from outshine_ref.buildings.modelling.radial_profile import RadialProfile
from outshine_ref.buildings.modelling.radial_mesh import radial_mesh, finish


def revolved(p,sides):
    values=np.asarray(p.knots);outer=values[:,[1,0]]
    inner=values[::-1][:,[2,0]] if p.hollow else np.array([[0,values[-1,0]],[0,values[0,0]]])
    line=np.concatenate((outer,inner,outer[:1]));keep=np.r_[True,np.any(np.diff(line,axis=0)!=0,axis=1)]
    return trimesh.creation.revolve(line[keep],sections=sides)


def standard_bound(p,sides):
    standard=revolved(p,sides);points=standard.vertices.copy();points[:,:2]+=p.centre_xy
    faces=standard.faces.astype(np.uint32);triangles=points[faces];values=np.asarray(p.knots)
    low=np.min(triangles[:,:,2],axis=1);high=np.max(triangles[:,:,2],axis=1)
    bands=np.searchsorted(values[:,0],low);radius=np.linalg.norm(triangles[:,:,:2]-p.centre_xy,axis=2)
    outer=np.interp(triangles[:,:,2],values[:,0],values[:,1]);inner=np.interp(triangles[:,:,2],values[:,0],values[:,2])
    roles=(np.sum(abs(radius-inner),axis=1)<np.sum(abs(radius-outer),axis=1)).astype(np.uint8) if p.hollow else np.zeros(len(faces),dtype=np.uint8)
    caps=low==high;roles[caps]=np.where(low[caps]==values[0,0],3,2)
    return finish(p,points,faces,roles,bands,values,tuple(range(len(values))),sides)


def case(p):
    native=radial_mesh(p,sides=32);standard=revolved(p,32)
    if not standard.is_watertight or not standard.is_winding_consistent:
        raise ValueError('Trimesh comparison did not build the same closed profile quality')
    if not math.isclose(native.volume_m3,standard.volume,rel_tol=1e-10,abs_tol=1e-8) or not math.isclose(native.area_m2,standard.area,rel_tol=1e-10,abs_tol=1e-8):
        raise ValueError('Profile comparison changed volume or surface')
    bound=standard_bound(p,32)
    if len(bound.mesh['faces'])!=len(native.mesh['faces']) or Counter(bound.mesh['materials'])!=Counter(native.mesh['materials']):
        raise ValueError('Bound comparison changed required geometry or material roles')
    repeats={}
    for name in ('numpy-bound-products','trimesh-bound-products'):
        samples=[]
        for i in range(24):
            started=time.perf_counter()
            mesh=radial_mesh(p,sides=32) if name.startswith('numpy') else standard_bound(p,32)
            if i>=3:samples.append((time.perf_counter()-started)*1000)
        repeats[name]=dict(p50_ms=float(np.percentile(samples,50)),p95_ms=float(np.percentile(samples,95)),repeats=21)
    return dict(identifier=p.identifier,knots=len(p.knots),triangles=len(native.mesh['faces']),
        volume_m3=native.volume_m3,surface_m2=native.area_m2,trimesh_triangles=len(standard.faces),timings=repeats,
        quality='Same profile/angular samples and closed volume/area; both emit root/UV/normal/material buffers. No global optimum claim.')


def main(output,input_path):
    controls=[RadialProfile('solid-cylinder',(0,0),((0,3,0),(12,3,0))),
              RadialProfile('cone',(0,0),((0,3,0),(12,0,0))),
              RadialProfile('hollow-shaft',(0,0),((0,3,2.6),(12,2.1,1.7)))]
    source=json.loads(input_path.read_text());chosen=[]
    for site,kind in [('Spittelau','chimney'),('Donauturm','mast'),('Grohnde','cooling_tower'),('Grohnde','industrial_round_body')]:
        rows=[f for f in source['features'] if f['site']==site and f['decision']['kind']==kind and not f['clipped']]
        item=max(rows,key=lambda f:f['decision']['metrics']['height_m']);raw=item['decision'].copy()
        raw['metrics']=StructureMetrics(**raw['metrics']);raw['alternatives']=tuple(raw['alternatives']);raw['evidence_ids']=tuple(raw['evidence_ids'])
        p,info=profile(item['id'],shapely.from_wkb(item['polygon_wkb']),StructureDecision(**raw))
        p=RadialProfile(p.identifier,(0,0),p.knots,p.materials);controls.append(p);chosen.append(info)
    results=[case(p) for p in controls]
    tiny=RadialProfile('tiny-valid-tube',(0,0),((0,1e-4,8e-5),(.01,1e-4,8e-5)))
    own=radial_mesh(tiny);other=revolved(tiny,32)
    if other.is_watertight:raise ValueError('Expected local Trimesh area-cutoff counterexample changed; re-evaluate backend choice')
    status=Path('/proc/self/status').read_text();peak=int(next(line.split()[1] for line in status.splitlines() if line.startswith('VmHWM:')))/1024
    result=revision()|dict(status='pass',cases=results,source_prototypes=chosen,
        process_peak_rss_mib=peak,peak_measurement='Linux /proc/self/status VmHWM, current address space; not inherited shell ru_maxrss',
        tiny_valid_counterexample=dict(own_triangles=len(own.mesh['faces']),trimesh_triangles=len(other.faces),trimesh_watertight=False,
            reason='Trimesh.revolve removes resolvable small cap triangles using its absolute tol.merge area cutoff'),
        scope='Python complete radial product construction and comparison; no native frame/cache/device proof')
    output.mkdir(parents=True,exist_ok=True);(output/'radial-verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(status='pass',cases=len(controls),peak_mib=result['process_peak_rss_mib'])))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/radial-forms'))
    parser.add_argument('--input',type=Path,default=Path('build/experiments/buildings/special-structures/classification.json'))
    args=parser.parse_args();main(args.output,args.input)
