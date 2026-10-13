"""Compare inherited and per-part scope projections on identical split facade geometry."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import drjit as dr
import mitsuba as mi
import numpy as np

from evidence import revision
from outshine_ref.buildings.modelling.split_sizes import Part
from modelling_uv_cases import adjacent
from mitsuba_material import reference
from mitsuba_mesh import mesh
from procedural_surface import panels


def probe(output,provenance):
    output.mkdir(parents=True,exist_ok=True)
    mi.set_variant('llvm_ad_rgb');dr.set_thread_count(2)
    material,textures,recipe=panels();bsdf,_,_=reference(material,textures)
    source=adjacent().s(6.,3.,0.).setup_projection(0,'scope.xy',2.4,1.2)
    parts=source.split('x',Part(2.),Part(4.));products=[]
    scene=dict(type='scene',integrator=dict(type='path'),sky=dict(type='constant',radiance=.6),
        sun=dict(type='directional',direction=[.4,-.3,-1.],irradiance=1.5),
        sensor=dict(type='perspective',to_world=mi.ScalarTransform4f().look_at(origin=[3.,3.3,10.],target=[3.,3.3,0.],up=[0,1,0]),
            fov=68,film=dict(type='hdrfilm',width=1920,height=1080),sampler=dict(type='independent',sample_count=32)))
    for row in range(2):
        for index,part in enumerate(parts):
            configured=part if row==1 else part.setup_projection(0,'scope.xy',2.4,1.2)
            emitted=configured.project_uv(0).t(0.,row*3.6,0.).emit()
            name='row-'+str(row)+'-part-'+str(index)
            scene[name],binding=mesh(name,emitted,np.arange(len(emitted['faces'])),bsdf)
            products.append(dict(name=name,mode='inherited' if row==1 else 'reset per part',**binding))
    loaded=mi.load_dict(scene);dr.sync_thread();start=time.perf_counter()
    image=mi.render(loaded,spp=32,seed=17);dr.eval(image);pixels=np.asarray(image);dr.sync_thread();elapsed=time.perf_counter()-start
    if not np.all(np.isfinite(pixels)):raise ValueError('Nonfinite projection rendering')
    image_path=output/'current-projection-continuity.png';mi.util.write_bitmap(str(image_path),image,write_async=False)
    record=provenance|dict(status='pass',profile=[1920,1080],spp=32,products=products,recipe=recipe,
        representation='showcase-current',page='buildings',view='components',label='projection',completed_recipe_families=0,
        renderer=dict(name='Mitsuba '+mi.__version__,backend='LLVM CPU',threads=2),render_seconds=elapsed,
        title='Texturprojektion · Vererbung und lokale Neuanlage',
        image_sha256=hashlib.sha256(image_path.read_bytes()).hexdigest(),
        caption='Oben: vor der Teilung eingerichtete Projektion läuft durch. Unten: Neuanlage je Teilfläche startet bei x=2 m erneut. Gleiche Geometrie; prozedurale Platten mit 2,4×1,2 m.',
        limits='Own analytical scope.xy implementation; diagnostic facade plane, no runtime PRT projection oracle or filtered LOD.')
    report='current-projection-continuity-report.json'
    (output/report).write_text(json.dumps(record,indent=2)+'\n');print(record['caption'])
    return [report]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/projection-development'))
    args=parser.parse_args();probe(args.output,revision())


if __name__=='__main__':main()
