"""CPU physical-light inspection of already cached buildings, with separate room-light state."""

import argparse
import hashlib
import json
from pathlib import Path
import time
import tempfile

import drjit as dr
import mitsuba as mi
import numpy as np

from mitsuba_mesh import mesh
from mitsuba_camera import camera
from mitsuba_material import reference
from modelling_metadata import Material
from modelling_material_schema import linear_to_srgb
from denoise_reference import denoise


def materials(config):
    colours=[(.56,.48,.36),config['wall_linear'],(.72,.73,.74),(.9,.96,1.),(.34,.38,.34),(.41,.30,.18),(.50,.41,.30),(.09,.12,.13),(.35,.35,.35),
             config['roof_linear'],(.16,.19,.21),(.43,.47,.49)]
    result={}
    for index,colour in enumerate(colours):
        if index==3:value=dict(type='dielectric',int_ior=1.52,ext_ior=1.)
        else:
            attributes={'color.'+axis:float(component) for axis,component in zip(('r','g','b'),linear_to_srgb(colour),strict=True)}
            attributes.update(shader='CityEnginePBRShader',roughness=.68,metallic=.4 if index in (2,7,10,11) else 0.)
            attributes.update(config.get('material_overrides',{}).get(str(index),{}))
            value,_,_=reference(Material('',attributes))
        result[index]=(value,None)
    for index,values in config.get('material_definitions',{}).items():result[int(index)]=reference(Material('',values))[:2]
    return result


def room_lights(config):
    lights={};records=[]
    for index,row in enumerate(config.get('room_lights',())):
        if not row['enabled']:continue
        transform=mi.ScalarTransform4f().translate(row['position']).rotate([1,0,0],180).scale([row['size'][0]/2,row['size'][1]/2,1.])
        lights['room-light-'+str(index)]=dict(type='rectangle',to_world=transform,bsdf=dict(type='diffuse',reflectance=0.),emitter=dict(type='area',radiance=dict(type='rgb',value=row['radiance'])))
        records.append(row)
    return lights,records


def render(config):
    mi.set_variant('llvm_ad_rgb');dr.set_thread_count(2);started=time.perf_counter()
    source=Path(config['mesh']);encoded=source.read_bytes()
    if hashlib.sha256(encoded).hexdigest()!=config['mesh_sha256']:raise ValueError('Cached interior source mesh changed')
    data=dict(np.load(source));palette=materials(config);spp=config.get('spp',64);sensor,projection=camera(config,spp)
    path_integrator=dict(type='path',max_depth=12,rr_depth=5)
    integrator=dict(type='aov',aovs='albedo:albedo,normal:sh_normal',image=path_integrator) if config.get('denoise') else path_integrator
    scene_data=dict(type='scene',integrator=integrator,sensor=sensor,
                    sky=dict(type='constant',radiance=dict(type='rgb',value=[.35,.4,.48] if not config.get('night') else [.001,.002,.004])))
    if not config.get('night'):scene_data['sun']=dict(type='directional',direction=[-.4,-.5,-1.],irradiance=dict(type='rgb',value=[3.,2.8,2.5]))
    bindings=[]
    for index in np.unique(data['materials']):
        rows=np.flatnonzero(data['materials']==index)
        if index==3 and config.get('glass')=='hidden':continue
        name='native-material-'+str(index);scene_data[name],info=mesh(name,data,rows,*palette[int(index)]);bindings.append(dict(material=int(index),**info))
    lights,light_state=room_lights(config);scene_data.update(lights)
    scene_data['ground']=dict(type='rectangle',to_world=mi.ScalarTransform4f().translate([0,0,-.025]).scale([config.get('ground_span_m',100.),config.get('ground_span_m',100.),1.]),
                             bsdf=dict(type='diffuse',reflectance=dict(type='rgb',value=[.16,.18,.16])))
    scene=mi.load_dict(scene_data);dr.sync_thread();setup_seconds=time.perf_counter()-started;render_start=time.perf_counter()
    image=mi.render(scene,spp=spp,seed=config.get('seed',17));dr.eval(image);pixels=np.asarray(image);dr.sync_thread()
    render_seconds=time.perf_counter()-render_start
    if pixels.shape[:2]!=(config['profile'][1],config['profile'][0]) or not np.isfinite(pixels).all():raise ValueError('Physical rendering is incomplete or non-finite')
    output=Path(config['output']);filter_info=None
    if config.get('denoise'):
        layers={name:np.asarray(bitmap) for name,bitmap in scene.sensors()[0].film().bitmap().split()}
        if not {'image','albedo','normal'}<=layers.keys():raise ValueError('Mitsuba is missing named denoising channels')
        raw=layers['image'][:,:,:3]
        if not np.allclose(raw,pixels[:,:,:3],rtol=1e-6,atol=1e-7):raise ValueError('Named radiance disagrees with rendered RGB')
        mi.util.write_bitmap(str(output.with_name(output.stem+'-raw.png')),raw)
        filter_log=Path(tempfile.gettempdir())/('outshine-buildings-'+output.stem+'-'+config['mesh_sha256'][:12]+'.oidn.log')
        pixels,filter_info=denoise(raw,layers['albedo'],layers['normal'],output.with_suffix(''),filter_log)
    mi.util.write_bitmap(str(output),pixels)
    record=dict(renderer='Mitsuba '+mi.__version__,backend='LLVM CPU',threads=dr.thread_count(),profile=config['profile'],spp=spp,seed=config.get('seed',17),
        setup_seconds=setup_seconds,render_seconds=render_seconds,mesh_sha256=config['mesh_sha256'],projection=projection,bindings=bindings,
        included_source_triangles=sum(row['triangles'] for row in bindings),source_triangles=len(data['faces']),room_light_state=light_state,
        glass='closed dielectric with procedural IOR 1.52' if config.get('glass')!='hidden' else 'explicit no-glass control',
        linear_rgb_mean=pixels[:,:,:3].mean((0,1)).tolist(),image_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),denoise=filter_info,
        limits='Complete cached control building; procedural material/light defaults, finite-sample CPU path tracing; no architecture family, native runtime, LOD or A18 admission')
    output.with_suffix('.renderer.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',**{key:record[key] for key in ('profile','spp','setup_seconds','render_seconds')})))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('scene',type=Path);args=parser.parse_args();render(json.loads(args.scene.read_text()))
