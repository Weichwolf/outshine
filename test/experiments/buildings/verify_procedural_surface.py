"""Check generated map values, repeat sampling, factors and deterministic resource identity."""

import argparse
import json
from pathlib import Path

import drjit as dr
import mitsuba as mi
import numpy as np

from evidence import revision
from mitsuba_material import bitmap,reference
from outshine_ref.buildings.modelling.material_schema import srgb_to_linear
from procedural_surface import encode,panels


def controls():
    mi.set_variant('llvm_ad_rgb');dr.set_thread_count(2)
    material,textures,info=panels(resolution=64)
    repeated=panels(resolution=64)
    assert textures==repeated[1] and info==repeated[2]
    changed=panels(resolution=64,seed=42)[1]
    assert changed[0].encoded!=textures[0].encoded and changed[2].encoded==textures[2].encoded
    bsdf,_,_=reference(material,textures);mi.load_dict(bsdf)
    pixels=np.array([[[.5,.25,.75],[1.,0.,0.]],[[0.,1.,0.],[0.,0.,1.]]])
    resource=encode('control',pixels);factor=np.array([.3,.5,.7])
    colour=mi.load_dict(bitmap(resource.path,(resource,),'color',factor))
    scalar=mi.load_dict(bitmap(resource.path,(resource,),'roughness',.6))
    si=mi.SurfaceInteraction3f()
    for row in range(2):
        for col in range(2):
            quantized=np.rint(pixels[row,col]*255)/255
            for offset in (0,2,-3):
                si.uv=[(col+.5)/2+offset,(row+.5)/2]
                np.testing.assert_allclose(np.asarray(colour.eval_3(si)).ravel(),srgb_to_linear(quantized)*factor,atol=1e-6)
                np.testing.assert_allclose(np.asarray(scalar.eval_1(si)).ravel(),quantized[0]*.6,atol=1e-6)
    flat=panels(resolution=64,depth_m=0)[1][2]
    normal=mi.load_dict(bitmap(flat.path,(flat,),'normal'))
    np.testing.assert_allclose(np.asarray(normal.eval_3(si)).ravel(),[128/255,128/255,1.],atol=1e-6)
    return dict(deterministic_bytes=True,seed_changes_colour_not_joint_geometry=True,
        generated_material_loads=True,srgb_once=True,scalar_linear=True,factors_applied=True,
        repeat_and_uv_orientation=True,flat_normal_quantization=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/procedural-surface-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',controls=controls(),
        limits='Base-level texture controls; no filtered LOD, native performance or complete CGA shader equivalence.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(record['controls']))
