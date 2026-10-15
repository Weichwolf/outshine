"""Same closed primitive and PBR closures with point versus projected-box facade coverage."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

import numpy as np
import shapely

from evidence import revision
from provider_contract import attributes
from tile_buildings import prism
from tile_facades import project


def run(output,spp):
    output.mkdir(parents=True,exist_ok=True);row=dict(id='facade-physical-control',polygon=shapely.box(0,0,20,10),properties=dict(render_height=15.,render_min_height=0.))
    p=attributes(row['properties']);before,_=prism(row,p,12);mesh,selected,facades=project(before,row,p,(.48,.53),3.2,3.)
    mesh['materials'][selected]=32768;path=output/'facade-control-mesh.npz';np.savez_compressed(path,**mesh);records=[]
    for pose,eye in [('near',[27.,-34.,24.]),('far',[27.,-1500.,24.])]:
        for filtering in (False,True):
            name=f"facade-{pose}-{'box' if filtering else 'point'}";image=output/(name+'.png')
            config=dict(mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),profile=[1920,1080],
                eye=eye,target=[10.,5.,7.5],fov_deg=48.,spp=spp,seed=17,denoise=True,output=str(image.resolve()),
                wall_linear=[.68,.67,.64],roof_linear=[.22,.24,.25],ground_span_m=3000.,
                facade_materials={'32768':dict(wall_material=12,fractions=[.48,.53],filtered=filtering)})
            scene=output/(name+'-scene.json');scene.write_text(json.dumps(config,indent=2)+'\n')
            log=Path(tempfile.gettempdir())/('outshine-buildings-'+name+'-'+revision()['commit'][:9]+'.log')
            with log.open('w') as stream:subprocess.run([sys.executable,str(Path(__file__).with_name('mitsuba_interior.py')),str(scene)],stdout=stream,stderr=subprocess.STDOUT,check=True)
            report=json.loads(image.with_suffix('.renderer.json').read_text());records.append(dict(pose=pose,filtered=filtering,scene=scene.name,
                image=image.name,mesh_sha256=config['mesh_sha256'],image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),renderer=report))
            print(name,'pass',flush=True)
    (output/'physical-controls.json').write_text(json.dumps(revision()|dict(status='pass',cases=records,
        scope='Same closed source geometry and live physical closures, point versus explicit camera tangent-box filter; estimated coarse window/room response, no true near openings/native EWA or device admission'),indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/facade-controls'))
    parser.add_argument('--spp',type=int,default=32);args=parser.parse_args();run(args.output,args.spp)
