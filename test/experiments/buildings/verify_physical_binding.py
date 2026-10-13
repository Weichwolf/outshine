"""Check named RGB guides, actual camera rays and mesh buffers independently of interior lighting."""

import argparse
import json
from pathlib import Path

import drjit as dr
import mitsuba as mi
import numpy as np

from evidence import revision
from mitsuba_camera import camera
from mitsuba_material import reference
from mitsuba_mesh import mesh
from modelling_metadata import Material


def controls():
    mi.set_variant('llvm_ad_rgb');dr.set_thread_count(2)
    expected=np.array([.21404114048223255,0.,1.])
    bsdf,emitter,info=reference(Material('',{'color.r':.5,'color.g':0.,'color.b':1.,'roughness':.4,'metallic':.2}))
    np.testing.assert_allclose(info['linear_base_colour'],expected,rtol=0,atol=1e-14);assert emitter is None
    sensor,projection=camera(dict(profile=[1920,1080],eye=[0.,0.,0.],target=[0.,0.,3.],fov_deg=50.),1)
    source=dict(vertices=np.array([[-10.,-10.,3.],[10.,-10.,3.],[10.,10.,3.],[-10.,10.,3.]]),faces=np.array([[0,2,1],[0,3,2]]))
    original=source['vertices'].copy()
    bound,binding=mesh('control',source,np.arange(2),dict(type='diffuse',reflectance=dict(type='rgb',value=[.2,.4,.6])))
    scene=mi.load_dict(dict(type='scene',sensor=sensor,plane=bound,
        integrator=dict(type='aov',aovs='albedo:albedo,normal:sh_normal',image=dict(type='path')),
        sky=dict(type='constant',radiance=1.)))
    pixels=np.asarray(mi.render(scene,spp=1));layers={key:np.asarray(value) for key,value in sensor.film().bitmap().split()}
    assert pixels.shape==(1080,1920,9)
    np.testing.assert_allclose(layers['image'][:,:,:3],pixels[:,:,:3],atol=1e-7)
    np.testing.assert_allclose(layers['albedo'][540,960],[.2,.4,.6],atol=1e-6)
    np.testing.assert_allclose(layers['normal'][540,960],[0.,0.,-1.],atol=1e-6)
    np.testing.assert_array_equal(source['vertices'],original)
    for key in ('colormap','normalmap','roughnessmap','metallicmap','opacitymap'):
        try:reference(Material('',{key:'unbound.png'}))
        except NotImplementedError:pass
        else:raise AssertionError('Unbound texture was silently ignored')
    return dict(actual_camera_rays=projection['ray_checks'],mesh_readback=binding,named_aov_channels=True,
        srgb_conversion=True,source_unchanged=True,unbound_textures_rejected=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/physical-binding-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',controls=controls(),limits='CPU reference binding only; full CityEngine shaders, renderer timing, temporal stability and native LOD remain open')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',controls=len(record['controls']))))
