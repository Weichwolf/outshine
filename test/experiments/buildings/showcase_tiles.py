"""Render four complete current provider tiles; detailed controls remain outside the public page."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import time

import numpy as np

from demand import View
from evidence import revision
from tile_buildings import SITES,construct,read_tile
from provider_contract import number,hidden


TITLES={'Spittelau':'Wien · Spittelau','BerlinFernsehturm':'Berlin · Fernsehturm','Grohnde':'Grohnde · Industriebauten','Hameln':'Hameln · Stadtgrundrisse'}


def camera(points):
    target=(points.min(axis=0)+points.max(axis=0))/2
    direction=np.array([1.,-1.25,.95]);direction/=np.linalg.norm(direction)
    view=View(target+direction,target,width=2560,height=1440,fov_deg=48.)
    projected=(points-target)@view.basis.T
    required=np.maximum(abs(projected[:,0])*view.focal_px/(2560*.46),abs(projected[:,1])*view.focal_px/(1440*.43))-projected[:,2]
    return View(target+direction*(required.max()+10.),target,width=2560,height=1440,fov_deg=48.)


def render(site,output,provenance,spp):
    start=time.perf_counter();rows,receipt=read_tile(site);boxes=[]
    for row in rows:
        if row['layer']!='building' or hidden(row['properties']):continue
        low=number(row['properties'].get('render_min_height',0.));high=number(row['properties'].get('render_height',9.))
        if high<=low:continue
        x0,y0,x1,y1=row['polygon'].bounds
        points=[(x,y,z) for x in (x0,x1) for y in (y0,y1) for z in (low,high)]
        boxes.append((row,high,points))
    focus=max(boxes,key=lambda b:b[1])[0]['polygon'].centroid
    selected=[points for row,high,points in boxes if row['polygon'].distance(focus)<350.]
    view=camera(np.concatenate([np.asarray(p) for p in selected]))
    projection=math.sqrt(1+(view.width/(2*view.focal_px))**2+(view.height/(2*view.focal_px))**2)
    def error_for(row,parameters):
        x0,y0,x1,y1=row['polygon'].bounds
        points=np.array([(x,y,z) for x in (x0,x1) for y in (y0,y1) for z in (parameters['minHeight'],parameters['buildingHeight'])])
        depth=float(np.min((points-view.eye)@view.basis[2]))
        return .5*depth/(view.focal_px*projection+.5) if depth>0 else .25
    mesh,source=construct(site,error_m=error_for,prepared=(rows,receipt));stem='current-tile-'+site.lower()
    path=output/(stem+'-mesh.npz');np.savez_compressed(path,**mesh)
    config=dict(profile=[2560,1440],mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        eye=view.eye.tolist(),target=view.target.tolist(),fov_deg=view.fov_deg,wall_linear=[.68,.67,.64],roof_linear=[.22,.24,.25],
        material_definitions=source['material_definitions'],ground_span_m=source['span_m']*1.5,
        spp=spp,seed=17,denoise=True,output=str((output/(stem+'.png')).resolve()))
    scene=output/(stem+'-scene.json');scene.write_text(json.dumps(config,indent=2)+'\n')
    log=Path(tempfile.gettempdir())/('outshine-buildings-'+stem+'-'+provenance['commit'][:9]+'.log')
    with log.open('w') as stream:
        subprocess.run([sys.executable,str(Path(__file__).with_name('mitsuba_interior.py')),str(scene)],stdout=stream,stderr=subprocess.STDOUT,check=True,timeout=900)
    image=output/(stem+'.png');renderer=json.loads(image.with_suffix('.renderer.json').read_text())
    if renderer['included_source_triangles']!=len(mesh['faces']):raise ValueError('Whole-tile renderer omitted triangles')
    report=provenance|dict(representation='showcase-current',view='tile',page='tile',title=TITLES[site],profile=[2560,1440],
        caption='Vollständige Kachelszene, näherer Blick auf die Bebauung. Sonderformen aus Quellhöhen und Grundrissen; übrige Gebäude zeigen ihre Grundvolumen.',
        renderer=renderer,source=source,triangles=len(mesh['faces']),build_seconds=time.perf_counter()-start,
        detail_policy='Source envelopes before emission; front-facing radial deviation <=0.5 projected pixel by minimum-depth bound; behind-camera complete profiles use 0.25 m',
        camera_scope='Near view of complete generated tile; source content is not cropped or culled before emission',
        image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),completed_recipe_families=0,
        limits='Structural prototypes; normal buildings remain prisms. Functional special recipes, terrain/transport/vegetation, native LOD/frame/device admission incomplete.')
    name=stem+'-report.json';(output/name).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(site=site,generated=source['generated'],triangles=len(mesh['faces']),status='pass')),flush=True)
    return name


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/tile-showcase'))
    parser.add_argument('--sites',nargs='+',default=list(SITES));parser.add_argument('--spp',type=int,default=32)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);provenance=revision()
    names=[render(site,args.output,provenance,args.spp) for site in args.sites]
    (args.output/'reports.json').write_text(json.dumps(dict(commit=provenance['commit'],reports=names),indent=2)+'\n')
