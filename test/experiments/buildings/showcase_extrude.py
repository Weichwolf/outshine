"""Display the current extrusion kernel on one shared roof and a real provider courtyard."""

import hashlib
import json
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from building_mesh import slice_triangles
from esri_oracle import CASES
from outshine_ref.buildings.modelling.extrude import MODES
from outshine_ref.buildings.modelling.faces import Face
from modelling_scope_probe import engine_mesh
from outshine_ref.buildings.modelling.shape import Shape
from preview import configuration,run_render
from provider_contract import attributes
from samples import read_site
from showcase_profile import PROFILE
from showcase_scene import fitted_points
from verify_modelling_extrude import volume


COLOURS=[(.08,.24,.65),(.85,.62,.10),(.75,.10,.09),(.12,.58,.22)]


def controls():
    points=np.array([[0,4,0],[4,4,0],[4,6,4],[0,6,4],[4,4,8],[0,4,8]],dtype=np.float64)
    source=Shape.from_faces(points,[Face((3,2,1,0),tags=('source-roof',)),Face((5,4,2,3),tags=('source-roof',))])
    cases=[(mode.replace('.','-'),mode+' · +3 m',source.extrude(3.,mode)) for mode in MODES]
    cases+=[('flat-negative','world.up.flatTop · −3 m',source.extrude(-3.,'world.up.flatTop')),
            ('flat-clamped','world.up.flatTop · 0 m → Dachspanne 2 m',source.extrude(0.,'world.up.flatTop'))]
    return source,cases


def semantic_mesh(body):
    mesh=engine_mesh(body.emit());mapping={1:12,2:13,4:14,8:15}
    return mesh|dict(materials=np.array([mapping[int(value)] for value in mesh['face_tag_mask']],dtype=np.uint16))


def save_report(output,stem,record):
    name=stem+'-report.json';(output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    return name


def render_control(output,provenance,key,title,body,camera,source=None):
    stem='current-extrude-'+key;mesh=semantic_mesh(body);path=output/(stem+'-mesh.npz');np.savez_compressed(path,**mesh)
    image=output/(stem+'.png');config,_=configuration([],camera,False,image)
    config.update(inspection_workbench=True,constructive=dict(mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                  wall_linear=[1.,1.,1.],roof_linear=[.155,.179,.196],extra_wall_colours=COLOURS))
    result=run_render(config,output/(stem+'-scene.json'))
    caption='Blau: Ausgangsfläche · Gelb: Außenwand · Rot: Hofwand · Grün: obere Fläche. Farben zeigen Komponententags; gespeicherte Quellmaterialien bleiben erhalten. '
    caption+='Echter OSM-Hofgrundriss und gelieferte Höhe.' if source else 'Dieselben zwei verbundenen geneigten Flächen, identische Kamera; Workbench-Studiolicht.'
    record=provenance|result|dict(representation='showcase-current',page='buildings',view='extrusion',label='modelling-extrude',
               title='Modellierkern · '+title,caption=caption,profile=PROFILE,triangles=len(mesh['faces']),
               logical_faces=len(body.geometry.polygons),raw_mesh_bytes=sum(array.nbytes for array in mesh.values()),volume_m3=volume(body.emit()),
               geometry_scope='polygon extrusion function control; no architecture recipe',completed_recipe_families=0,
               lod_status='complete control geometry; native LOD unproved',scope_size_m=body.scope.size)
    if source:record.update(source_id=source['id'],source_sha256=source['source_sha256'],source_holes=len(source['polygon'].interiors))
    print(json.dumps(dict(page='buildings',extrusion=key,seconds=result['render_seconds'])),flush=True)
    return save_report(output,stem,record)


def plan_figure(source,cases,output,provenance):
    began=time.perf_counter();fig,axes=plt.subplots(2,3,figsize=(15,9),sharex=True,sharey=True)
    for ax,(_,title,body) in zip(axes.flat,cases,strict=True):
        mesh=engine_mesh(body.emit())
        for segment,_ in slice_triangles(mesh,[1.,0.,0.],2.):ax.plot(segment[:,1],segment[:,2],color='#285b7e',linewidth=1.8)
        for segment,_ in slice_triangles(engine_mesh(source.emit()),[1.,0.,0.],2.):ax.plot(segment[:,1],segment[:,2],color='#e89d31',linewidth=2.)
        ax.set_title(title);ax.set_aspect('equal');ax.grid(alpha=.2);ax.set_xlabel('Nord [m]');ax.set_ylabel('Höhe [m]')
    fig.suptitle('Extrusion derselben verbundenen Dachflächen · metrische Schnitte bei Ost = 2 m\nOrange: Ausgangsdach · Blau: erzeugte Volumengrenze',fontsize=13)
    fig.text(.02,.015,'Funktionskontrollen, keine fertigen Gebäude-Rezepte. Negative Distanz und Flat-Top-Kontakt am First ausdrücklich enthalten. '+provenance['commit'][:9],fontsize=9)
    fig.tight_layout(rect=(0,.035,1,.93));fig.savefig(output,dpi=100);plt.close(fig)
    return time.perf_counter()-began


def probe(output,provenance):
    source,cases=controls();points=np.concatenate([engine_mesh(body.emit())['vertices'] for _,_,body in cases])
    camera=fitted_points(points,PROFILE);reports=[render_control(output,provenance,key,title,body,camera) for key,title,body in cases]
    _,site,identifier=CASES[1];sample=next(row for row in read_site(site)[0] if row['id']==identifier);data=attributes(sample['properties'])
    body=Shape.from_polygon(sample['polygon'],data['minHeight'],tags=(identifier,)).extrude(data['bodyHeight'],'world.up')
    reports.append(render_control(output,provenance,'vienna-courtyard','Wien · Hoföffnung und Extrusion',body,
                                  fitted_points(engine_mesh(body.emit())['vertices'],PROFILE),sample))
    stem='current-extrude-sections';image=output/(stem+'.png');seconds=plan_figure(source,cases,image,provenance)
    reports.append(save_report(output,stem,provenance|dict(representation='showcase-current',page='buildings',view='plan',
        label='modelling-extrude',title='Modellierkern · Extrusion im Schnitt',caption='Vier Extrusionsarten, negative Distanz und minimale Flat-Top-Höhe am selben Dach. Metrische 2D-Schnitte der tatsächlich erzeugten Dreiecke.',
        renderer='Metric Matplotlib',profile=[1500,900],render_seconds=seconds,image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
        completed_recipe_families=0,lod_status='geometry sections; native LOD unproved',measurement_scope='CPU plotting/PNG generation, not frame duration')))
    return reports
