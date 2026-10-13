"""Six metric projection frames on identical closed solids, with visible depth shear."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import drjit as dr
import mitsuba as mi
import numpy as np
from PIL import Image,ImageDraw
import shapely

from evidence import revision
from outshine_ref.buildings.modelling.shape import Shape
from mitsuba_material import reference
from mitsuba_mesh import mesh
from procedural_surface import panels


def probe(output,provenance):
    output.mkdir(parents=True,exist_ok=True)
    mi.set_variant('llvm_ad_rgb');dr.set_thread_count(2)
    material,textures,recipe=panels();bsdf,_,_=reference(material,textures)
    body=Shape.from_polygon(shapely.box(0.,0.,4.,2.)).extrude(3.)
    canvas=Image.new('RGB',(5760,2160));products=[]
    for index,axes in enumerate(('xy','xz','yx','yz','zx','zy')):
        emitted=body.setup_projection(0,'scope.'+axes,2.4,1.2,uw_factor=.35).project_uv(0).emit()
        solid,binding=mesh('solid',emitted,np.arange(len(emitted['faces'])),bsdf)
        camera=mi.ScalarTransform4f().look_at(origin=[8.,7.,9.],target=[2.,1.5,-1.],up=[0,1,0])
        positions=np.column_stack((emitted['vertices'],np.ones(len(emitted['vertices']))))
        view=positions@np.asarray(camera.inverse().matrix).T
        projected=view[:,:2]/view[:,2,None]/np.tan(np.deg2rad(21.))
        projected[:,1]*=1920/1080
        if np.any(view[:,2]<=0) or np.any(abs(projected)>[.9,.85]):
            raise ValueError('Projection comparison crops the solid or its title margin')
        scene=dict(type='scene',integrator=dict(type='path'),solid=solid,
            sky=dict(type='constant',radiance=.6),
            sun=dict(type='directional',direction=[-.4,-.8,-1.],irradiance=1.5),
            sensor=dict(type='perspective',to_world=camera,fov=42,
                film=dict(type='hdrfilm',width=1920,height=1080),sampler=dict(type='independent',sample_count=32)))
        loaded=mi.load_dict(scene);dr.sync_thread();start=time.perf_counter()
        rendered=mi.render(loaded,spp=32,seed=17);dr.eval(rendered);dr.sync_thread()
        elapsed=time.perf_counter()-start
        if not np.isfinite(np.asarray(rendered)).all():raise ValueError('Nonfinite projection image')
        bitmap=mi.Bitmap(rendered).convert(mi.Bitmap.PixelFormat.RGB,mi.Struct.Type.UInt8,True)
        tile=Image.fromarray(np.asarray(bitmap));draw=ImageDraw.Draw(tile)
        draw.rectangle((0,0,1920,70),fill='#17242b')
        draw.text((30,18),'scope.'+axes+'   |   2.4 x 1.2 m   |   depth shear 0.35',fill='white',font_size=32)
        canvas.paste(tile,((index%3)*1920,(index//3)*1080))
        products.append(dict(selector='scope.'+axes,**binding,render_seconds=elapsed,
                             projected_bounds=[projected.min(axis=0).tolist(),projected.max(axis=0).tolist()]))
    image_path=output/'current-projection-axes.png';canvas.save(image_path)
    record=provenance|dict(status='pass',profile=[5760,2160],tile_profile=[1920,1080],spp=32,
        products=products,recipe=recipe,representation='showcase-current',page='buildings',view='components',
        label='projection-axes',completed_recipe_families=0,
        renderer=dict(name='Mitsuba '+mi.__version__,backend='LLVM CPU',threads=2),
        render_seconds=sum(row['render_seconds'] for row in products),
        title='Sechs Scope-Projektionen · Fassade und Dach',
        image_sha256=hashlib.sha256(image_path.read_bytes()).hexdigest(),
        caption='Identische 4×3×2-m-Körper und Kameras. Projektion 2,4×1,2 m, Tiefenscherung 0,35; XY/XZ/YX oben, YZ/ZX/ZY unten. Achsenparallele Flächen zeigen bewusst die Projektionsstreckung.',
        limits='Six 1920×1080 diagnostic tiles; own scope implementation with static SDK evidence, no executable PRT oracle, architecture recipe or native LOD approval.')
    report='current-projection-axes-report.json'
    (output/report).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(report=report,render_seconds=record['render_seconds'])),flush=True)
    return [report]


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/projection-axes-development'))
    args=parser.parse_args();probe(args.output,revision())
