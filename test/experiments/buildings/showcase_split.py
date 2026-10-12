"""Inspect complete closed cut pieces, source courtyards and actual generated cut surfaces."""

from dataclasses import asdict
import hashlib
import json
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import numpy as np

from esri_oracle import CASES
from modelling_shape import Shape
from modelling_split_sizes import Part,Floating
from modelling_scope_probe import engine_mesh
from preview import configuration,run_render
from provider_contract import attributes
from samples import read_site
from showcase_groups import COLOURS,inspection_mesh,save_plot
from showcase_scene import fitted_points
from verify_modelling_groups import cube


def render_control(output,provenance,key,title,children,camera,sample=None):
    stem='current-cuts-'+key;mesh=inspection_mesh([(child,i%4) for i,child in enumerate(children)])
    path=output/(stem+'-mesh.npz');np.savez_compressed(path,**mesh)
    image=output/(stem+'.png');config,_=configuration([],camera,False,image)
    config.update(inspection_workbench=True,constructive=dict(mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        wall_linear=[1.,1.,1.],roof_linear=[.155,.179,.196],extra_wall_colours=COLOURS))
    rendered=run_render(config,output/(stem+'-scene.json'))
    record=provenance|rendered|dict(representation='showcase-current',page='buildings',view='components',label='modelling-split',
        title='Modellierkern · '+title,caption='Vollständige triangulierte Teilkörper, für die Ansicht auseinandergezogen. '
        'Schnittflächen sind geschlossen; Quellmaterial, Gruppen, Tags und Corner-UVs erhalten. '
        'Opake Prüfpalette und Workbench-Studiolicht; noch keine Architektur-Rezeptfamilie.',
        profile=[1280,720],triangles=len(mesh['faces']),raw_mesh_bytes=sum(a.nbytes for a in mesh.values()),
        components=len(children),split_parts=[asdict(child.split_info) for child in children],
        completed_recipe_families=0,lod_status='native LOD unproved',
        geometry_scope='complete cut solid geometry including generated closure surfaces',
        measurement_scope='Workbench inspection render; no native frame timing')
    if sample:record.update(source_id=sample['id'],source_sha256=sample['source_sha256'],source_holes=len(sample['polygon'].interiors))
    name=stem+'-report.json';(output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(page='buildings',control=key,seconds=rendered['render_seconds'])),flush=True);return name


def section_plot(children,output,provenance):
    start=time.perf_counter();fig=plt.figure(figsize=(13,8));ax=fig.add_subplot(111,projection='3d')
    points=[]
    for i,child in enumerate(children):
        mesh=engine_mesh(child.emit());points.append(mesh['vertices']);triangles=mesh['vertices'][mesh['faces']]
        ax.add_collection3d(Poly3DCollection(triangles,facecolors=COLOURS[i%4],edgecolors='#34404a',linewidths=.25,alpha=.75))
    points=np.concatenate(points);low,high=points.min(axis=0),points.max(axis=0);span=high-low
    ax.set_xlim(low[0],high[0]);ax.set_ylim(low[1],high[1]);ax.set_zlim(low[2],high[2]);ax.set_box_aspect(span)
    ax.set_xlabel('Ost [m]');ax.set_ylabel('Nord [m]');ax.set_zlabel('Höhe [m]');ax.view_init(elev=27,azim=-53)
    title='Wien · geschlossene Geschossblöcke mit durchgehendem Hof'
    fig.suptitle(title+'\nTatsächliche Dreiecke und geschlossene Schnittflächen · metrische 2.5D-Ansicht',fontsize=13)
    fig.text(.04,.02,'Teilvolumen sind Prüfgeometrie, keine begehbaren Innenräume. '+provenance['commit'][:9],fontsize=10)
    fig.subplots_adjust(left=.04,right=.96,bottom=.08,top=.88)
    return save_plot(fig,output,'current-cuts-section-plan',title,
        'Vier erzeugte Teilkörper eines echten Wiener Hofgrundrisses. Der Hof bleibt in jedem horizontalen Abschluss offen.',
        provenance,start,[1300,800],label='modelling-split')


def probe(output,provenance):
    source=cube();parts=source.split('y',Part(1.5,'floor'),repeat=True,cut_material=7,cut_group=8)
    exploded=tuple(child.t(0.,child.split_info.index*.8,0.) for child in parts)
    camera=fitted_points(np.concatenate([engine_mesh(child.emit())['vertices'] for child in exploded]),[1280,720])
    reports=[render_control(output,provenance,'cube-floors','Absolute Geschosse · 1,5 / 1,5 / 1 m',exploded,camera)]
    _,site,identifier=CASES[1];sample=next(row for row in read_site(site)[0] if row['id']==identifier);data=attributes(sample['properties'])
    body=Shape.from_polygon(sample['polygon'],data['minHeight'],material=7,tags=(identifier,)).extrude(data['bodyHeight'],'world.up')
    pattern=[Part(Floating(1.),str(i)) for i in range(4)]
    for axis,key,title in [('y','courtyard-floors','Wien · vier Geschossblöcke und Hof'),('x','courtyard-widths','Wien · vier räumliche Breitenteilungen')]:
        children=body.split(axis,*pattern);direction=np.zeros(3);direction[('x','y','z').index(axis)]=3.
        exploded=tuple(child.t(*(direction*child.split_info.index)) for child in children)
        camera=fitted_points(np.concatenate([engine_mesh(child.emit())['vertices'] for child in exploded]),[1280,720])
        reports.append(render_control(output,provenance,key,title,exploded,camera,sample))
        if axis=='y':reports.append(section_plot(exploded,output,provenance))
    return reports
