"""Current UV operation plans and real Workbench texture bindings on complete control geometry."""

import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image,ImageDraw

from modelling_scope_probe import engine_mesh
from modelling_uv_cases import adjacent,courtyard
from showcase_groups import save_plot
from showcase_profile import PROFILE


def texture(path):
    image=Image.new('RGB',(256,256),'#f8eed4');draw=ImageDraw.Draw(image)
    for bounds,colour in [((0,0,127,127),'#29a482'),((128,0,255,127),'#e26b2d'),((0,128,127,255),'#257ec3'),((128,128,255,255),'#e6bf39')]:draw.rectangle(bounds,fill=colour)
    for offset in (32,64,96,160,192,224):draw.line((offset,0,offset,255),fill='#f8eed4',width=2);draw.line((0,offset,255,offset),fill='#f8eed4',width=2)
    draw.rectangle((8,136,27,235),fill='#102a3b');draw.polygon([(38,228),(88,203),(88,253)],fill='#102a3b');image.save(path)


def record(output,stem,provenance,title,caption,renderer,seconds,extra):
    image=output/(stem+'.png');name=stem+'-report.json'
    report=provenance|dict(representation='showcase-current',page='buildings',view='components',label='modelling-uv',
        title=title,caption=caption,profile=PROFILE,image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),renderer=renderer,
        render_seconds=seconds,completed_recipe_families=0,lod_status='native LOD unproved',measurement_scope='texture inspection only; no native GPU/frame timing')|extra
    (output/name).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');return name


def textured(output,provenance,key,title,cases,flat=False):
    stem='current-uv-'+key;image=output/(stem+'.png');atlas=output/'owned-uv-atlas.png'
    if not atlas.exists():texture(atlas)
    positions=[];triangles=[];uv=[];offset=0;labels=[]
    for index,(label,shape) in enumerate(cases):
        x=(index%3)*3.6;y=3. if index<3 else 0.
        body=shape.t(x,y,0.) if not flat else shape
        mesh=engine_mesh(body.emit());positions.append(mesh['vertices']);triangles.append(mesh['faces']+offset);uv.append(mesh['uv']);offset+=len(mesh['vertices'])
        if not flat:labels.append(dict(text=label,position=[x,-.015,y-.3]))
    mesh_path=output/(stem+'-mesh.npz');np.savez_compressed(mesh_path,vertices=np.concatenate(positions),faces=np.concatenate(triangles),uv=np.concatenate(uv))
    config=dict(mesh=str(mesh_path.resolve()),texture=str(atlas.resolve()),mesh_sha256=hashlib.sha256(mesh_path.read_bytes()).hexdigest(),
        texture_sha256=hashlib.sha256(atlas.read_bytes()).hexdigest(),uv_channel='uv',profile=PROFILE,labels=labels,output=str(image.resolve()),
        eye=[3.,-6.,8.] if flat else [5.1,-30.,2.4],target=[3.,2.,0.] if flat else [5.1,0.,2.4],ortho_scale=9. if flat else 12.)
    scene=output/(stem+'-scene.json');scene.write_text(json.dumps(config,indent=2)+'\n');began=time.perf_counter()
    log=Path(tempfile.gettempdir())/('outshine-buildings-'+stem+'-'+provenance['commit'][:9]+'.log')
    with log.open('w') as stream:
        subprocess.run(['blender','--background','--factory-startup','--threads','2','--python-exit-code','1','--python',str(Path(__file__).with_name('render_uv_inspection.py')),'--',str(scene)],stdout=stream,stderr=subprocess.STDOUT,check=True,timeout=120)
    renderer=json.loads(image.with_suffix('.renderer.json').read_text())
    return record(output,stem,provenance,title,'Eigene asymmetrische Prüftextur auf den tatsächlich ausgegebenen UVs; Workbench ohne Materiallicht/PBR. '
        'Geometrie, Flächenbeziehungen und Aussparungen bleiben erhalten. Keine Architektur-Rezeptfamilie.',renderer,time.perf_counter()-began,
        dict(controls=[label for label,_ in cases],triangles=len(np.concatenate(triangles)),geometry_scope='complete control surfaces with actual texture coordinate binding',log=str(log)))


def plan(output,provenance):
    source=adjacent();cases=[('Quelle',source),('collectiveAllFaces',source.normalize_uv(0)),('separatePerFace',source.normalize_uv(0,'uv','separatePerFace'))]
    began=time.perf_counter();fig,axes=plt.subplots(1,3,figsize=(15,7))
    for ax,(title,shape) in zip(axes,cases,strict=True):
        for i,face in enumerate(shape.geometry.polygons):
            points=np.asarray(face.uvs[0]);ax.plot(*np.r_[points,points[:1]].T,color=('#207fb5','#ca5a3a')[i],marker='o',label=shape.geometry.group_names[i][1])
        ax.set_title(title);ax.set_aspect('equal');ax.grid(alpha=.2);ax.set_xlabel('u');ax.set_ylabel('v');ax.legend()
    fig.suptitle('UV-Normalisierung · gemeinsam genutzte Geometrie, getrennte Texturflächen',fontsize=14);fig.subplots_adjust(left=.05,right=.98,bottom=.14,top=.82,wspace=.3)
    fig.text(.03,.025,'Unabhängige analytische Kontrolle; Normalisierung verändert weder Positionen noch Räume/Materialgruppen. '+provenance['commit'][:9],fontsize=10)
    return save_plot(fig,output,'current-uv-normalize-plan','UVs · kollektive und flächenweise Normalisierung',
        'Zwei benachbarte Flächen vor und nach Normalisierung. Separate Flächen erhalten eigene UV-Einträge an der gemeinsamen Geometriekante.',provenance,began,[1500,700],label='modelling-uv')


def probe(output,provenance):
    base=adjacent().normalize_uv(0)
    affine=[('Original',base),('scaleUV (2, 1)',base.scale_uv(0,2,1)),('rotateUV 30 degrees',base.rotate_uv(0,30)),
            ('translateUV (.2, .3)',base.translate_uv(0,.2,.3)),('scaleUV (-1, 1)',base.scale_uv(0,-1,1)),('copy UV0 to UV1 to UV0',base.copy_uv(0,1).delete_uv(0).copy_uv(1,0))]
    source=adjacent();normal=[('Original (repeat)',source),('Collective UV',source.normalize_uv(0)),('Per face UV',source.normalize_uv(0,'uv','separatePerFace')),
          ('Collective UV uniform',source.normalize_uv(0,'uvUniform')),('Per face UV uniform',source.normalize_uv(0,'uvUniform','separatePerFace')),('Per face U only',source.normalize_uv(0,'u','separatePerFace'))]
    return [plan(output,provenance),textured(output,provenance,'affine','UVs · Skalierung, Rotation, Translation und Kanal-Kopie',affine),
            textured(output,provenance,'normalization','UVs · Flächenweise Normalisierung erhält Texturgrenzen',normal),
            textured(output,provenance,'courtyard','UVs · Hoföffnung und normalisierte Randkoordinaten',[('Hof',courtyard().normalize_uv(0,'uv','separatePerFace'))],flat=True)]
