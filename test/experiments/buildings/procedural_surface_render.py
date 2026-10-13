"""Render a metric panel control with generated colour, roughness and shading relief."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import drjit as dr
import mitsuba as mi
import numpy as np

from evidence import revision
from mitsuba_material import reference
from mitsuba_mesh import mesh
from procedural_surface import panels


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/texture-development'))
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    mi.set_variant('llvm_ad_rgb');dr.set_thread_count(2)
    start=time.perf_counter();material,textures,info=panels();generation=time.perf_counter()-start
    bsdf,_,_=reference(material,textures)
    data=dict(vertices=np.array([[-2.4,-1.2,0],[2.4,-1.2,0],[2.4,1.2,0],[-2.4,1.2,0]]),
        faces=np.array([[0,1,2],[0,2,3]]),uv=np.array([[0,0],[2,0],[2,2],[0,2]],dtype=float))
    shape,binding=mesh('panels',data,np.arange(2),bsdf)
    scene=mi.load_dict(dict(type='scene',integrator=dict(type='path'),panel=shape,
        sky=dict(type='constant',radiance=.35),sun=dict(type='directional',direction=[.5,-.3,-1.],irradiance=2.),
        sensor=dict(type='perspective',to_world=mi.ScalarTransform4f().look_at(origin=[2.5,1.4,7],target=[0,0,0],up=[0,1,0]),
            fov=48,film=dict(type='hdrfilm',width=1920,height=1080),sampler=dict(type='independent',sample_count=32))))
    start=time.perf_counter();image=mi.render(scene,spp=32);dr.eval(image);pixels=np.asarray(image);dr.sync_thread()
    elapsed=time.perf_counter()-start
    if not np.all(np.isfinite(pixels)):raise ValueError('Nonfinite rendered pixels')
    output=args.output/'metric-panels.png';mi.util.write_bitmap(str(output),image,write_async=False)
    for texture in textures:(args.output/Path(texture.path).name).write_bytes(texture.encoded)
    record=revision()|dict(status='pass',profile=[1920,1080],spp=32,backend='Mitsuba LLVM RGB CPU, two threads',
        material=info,binding=binding,generation_s=generation,render_s=elapsed,image_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
        texture_sha256={texture.path:texture.sha256 for texture in textures},
        limits='Diagnostic plane, no building integration; shading relief only; no LOD, native frame or full CGA shader evidence')
    (args.output/'metric-panels.json').write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',generation_s=generation,render_s=elapsed)))


if __name__=='__main__':main()
