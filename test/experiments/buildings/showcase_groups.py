"""Inspect authored groups, equivalent materials and filled courtyard components."""

import hashlib
import json
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from esri_oracle import CASES
from modelling_components import Branch
from modelling_group_probe import authored
from modelling_scope_probe import engine_mesh
from modelling_selectors import tagged
from modelling_shape import Shape
from preview import configuration,run_render
from provider_contract import attributes
from samples import read_site
from showcase_scene import fitted_points
from verify_modelling_groups import cube


COLOURS=[(.13,.48,.72),(.75,.23,.14),(.17,.56,.34),(.85,.65,.16)]


def inspection_mesh(shapes):
    vertices=[];faces=[];materials=[];parts=[];offset=0
    for index,(shape,colour) in enumerate(shapes):
        mesh=engine_mesh(shape.emit());vertices.append(mesh['vertices']);faces.append(mesh['faces']+offset);offset+=len(mesh['vertices'])
        materials.append(np.full(len(mesh['faces']),12+colour,dtype=np.uint16));parts.append(np.full(len(mesh['faces']),index,dtype=np.int32))
    return dict(vertices=np.concatenate(vertices),faces=np.concatenate(faces),materials=np.concatenate(materials),parts=np.concatenate(parts))


def render_control(output,provenance,key,title,shapes,camera,sample=None):
    mesh=inspection_mesh(shapes);stem='current-groups-'+key;path=output/(stem+'-mesh.npz');np.savez_compressed(path,**mesh)
    image=output/(stem+'.png');config,_=configuration([],camera,False,image)
    config.update(inspection_workbench=True,constructive=dict(mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                  wall_linear=[1.,1.,1.],roof_linear=[.155,.179,.196],extra_wall_colours=COLOURS))
    result=run_render(config,output/(stem+'-scene.json'));components=[]
    for shape,_ in shapes:
        if shape.comp_info:
            info=shape.comp_info;components.append(dict(kind=info.kind,selector=info.sel,index=info.index,total=info.total,source_indices=info.source_indices,
                group_names=shape.group_names(),material_names=shape.material_names(),scope_size_m=shape.scope.size))
    report=provenance|result|dict(representation='showcase-current',page='buildings',view='components',label='modelling-groups',
        title='Modellierkern · '+title,caption='Farben zeigen die tatsächliche ausgewählte Gruppierung. Quellgruppen, Materialwerte, native Palette-IDs, UVs und Weltkoordinaten bleiben erhalten. '
        'Prüfpalette mit opaken Flächen und Workbench-Studiolicht; keine Architektur-Rezeptfamilie.',profile=[1280,720],triangles=len(mesh['faces']),
        components=len(components),component_groups=components,logical_faces=sum(len(shape.geometry.polygons) for shape,_ in shapes),
        source_holes=len(sample['polygon'].interiors) if sample else 0,raw_mesh_bytes=sum(a.nbytes for a in mesh.values()),completed_recipe_families=0,
        geometry_scope='complete selected group/material/hole geometry and declared inspection transforms',lod_status='native LOD unproved',
        measurement_scope='Workbench inspection render; no native frame timing')
    if sample:report.update(source_id=sample['id'],source_sha256=sample['source_sha256'],source_holes=len(sample['polygon'].interiors))
    name=stem+'-report.json';(output/name).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(dict(page='buildings',control=key,seconds=result['render_seconds'])),flush=True);return name


def save_plot(fig,output,stem,title,caption,provenance,start,profile):
    path=output/(stem+'.png');fig.savefig(path,dpi=100);plt.close(fig)
    report=provenance|dict(representation='showcase-current',page='buildings',view='plan',label='modelling-groups',title=title,caption=caption,
        renderer='Metric Matplotlib 2D',profile=profile,render_seconds=time.perf_counter()-start,image_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        completed_recipe_families=0,lod_status='native LOD unproved',measurement_scope='CPU metric plotting/PNG generation; no frame duration')
    name=stem+'-report.json';(output/name).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');return name


def face_plan(source,output,provenance):
    start=time.perf_counter();fig,axes=plt.subplots(2,3,figsize=(15,9));groups=dict(source.geometry.group_names);materials=dict(source.geometry.material_defs)
    for ax,child in zip(axes.flat,source.comp('f'),strict=True):
        mesh=child.emit();coordinates=(mesh['vertices']-child.scope.origin)@child.scope.axes;face=child.geometry.polygons[0]
        xy=coordinates[list(face.outer),:2];colour='#28738a' if face.group==8 else '#bc6b4f'
        ax.fill(*xy.T,color=colour,alpha=.35);ax.plot(*np.r_[xy,xy[:1]].T,color=colour,linewidth=1.5)
        for triangle in mesh['faces']:ax.plot(*np.r_[coordinates[triangle,:2],coordinates[triangle[:1],:2]].T,color='#89969a',linewidth=.6)
        ax.set_title('g='+str(face.group)+' · '+groups[face.group]+'\nm='+str(face.material)+' · '+materials[face.material].name)
        ax.set_aspect('equal');ax.grid(alpha=.2);ax.set_xlabel('Lokales x [m]');ax.set_ylabel('Lokales y [m]')
    title='Gruppen und Materialwerte · sechs Flächen im metrischen Plan'
    fig.suptitle(title+'\nQuellgruppen 8/32 · IDs 7/19 haben dieselben deklarierten stone-Werte',fontsize=13)
    fig.text(.02,.015,'Native Dreiecke und Corner-UVs erhalten; Farben markieren Gruppenzugehörigkeit. '+provenance['commit'][:9],fontsize=9)
    fig.subplots_adjust(left=.06,right=.97,bottom=.12,top=.85,wspace=.25,hspace=.45)
    return save_plot(fig,output,'current-groups-face-plan',title,'Jede Fläche in ihrem eigenen Scope; Quellgruppen und Material-Palette-IDs bleiben getrennt nachvollziehbar.',provenance,start,[1500,900])


def hole_plan(roof,holes,output,provenance):
    start=time.perf_counter();fig,axes=plt.subplots(1,2,figsize=(13,7));points=engine_mesh(roof.emit())['vertices'][:,:2]
    low,high=points.min(axis=0)-2.,points.max(axis=0)+2.;face=roof.geometry.polygons[0]
    axes[0].fill(*points[list(face.outer)].T,color='#28738a',alpha=.35)
    for ring in face.holes:axes[0].fill(*points[list(ring)].T,color='white')
    for number,ax in enumerate(axes):
        for ring in (face.outer,*face.holes):ax.plot(*np.r_[points[list(ring)],points[ring[0]][None]].T,color='#28738a',linewidth=1.5)
        if number:
            for index,hole in enumerate(holes):
                vertices=engine_mesh(hole.emit())['vertices'][:,:2];xy=vertices[list(hole.geometry.polygons[0].outer)]
                ax.fill(*xy.T,color='#34926d',alpha=.55);ax.text(*xy.mean(axis=0),'h='+str(index),ha='center',fontsize=11)
        ax.set_xlim(low[0],high[0]);ax.set_ylim(low[1],high[1]);ax.set_aspect('equal');ax.grid(alpha=.2);ax.set_xlabel('Ost [m]');ax.set_ylabel('Nord [m]')
        ax.set_title('Quellfläche mit Innenhof' if number==0 else 'comp(h) · gefüllte Hofkomponente')
    title='Wien · Quellfläche und extrahierter Innenhof'
    fig.suptitle(title+'\nGleicher metrischer Maßstab · Grün: tatsächliche triangulierte Lochkomponente',fontsize=13)
    fig.text(.02,.02,'Die ursprüngliche Dachfläche behält den Hof; die separate Lochkomponente ist eine gefüllte Fläche. '+provenance['commit'][:9],fontsize=9)
    fig.subplots_adjust(left=.08,right=.96,bottom=.1,top=.82,wspace=.22)
    return save_plot(fig,output,'current-groups-hole-plan',title,'Derselbe echte Grundriss: unveränderte Quellfläche mit Loch und separate gefüllte Auswahl samt UVs/Material/Gruppe.',provenance,start,[1300,700])


def probe(output,provenance):
    source=cube();groups=source.comp('g');materials=source.comp('m');moved=tuple(child.t(0.,0.,1.1) for child in groups)
    camera=fitted_points(np.concatenate([engine_mesh(child.emit())['vertices'] for child in (*groups,*moved)]),[1280,720])
    cases=[('cube-groups','Quellgruppen · frame und shell',groups),('cube-materials','Gemeinsame Materialwerte · stone und glass',materials),
           ('cube-moved','Gruppenscopes · t(0, 0, 1,1) je Gruppe',moved)]
    reports=[render_control(output,provenance,key,title,tuple((child,i) for i,child in enumerate(children)),camera) for key,title,children in cases]
    _,site,identifier=CASES[1];sample=next(row for row in read_site(site)[0] if row['id']==identifier);data=attributes(sample['properties'])
    body=authored(Shape.from_polygon(sample['polygon'],data['minHeight'],tags=(identifier,)).extrude(data['bodyHeight'],'world.up'))
    roof=body.comp('f',Branch(tagged('extrude.top')))[0];holes=roof.comp('h');markers=tuple(hole.extrude(2.,'world.up') for hole in holes)
    camera=fitted_points(np.concatenate([engine_mesh(child.emit())['vertices'] for child in (body,*markers)]),[1280,720])
    reports.append(render_control(output,provenance,'courtyard-groups','Wien · shell, roof und courtyard',tuple((child,i) for i,child in enumerate(body.comp('g'))),camera,sample))
    reports.append(render_control(output,provenance,'courtyard-holes','Wien · Hofauswahl mit +2 m Prüfextrusion',((body,0),*((marker,2) for marker in markers)),camera,sample))
    reports.append(face_plan(source,output,provenance));reports.append(hole_plan(roof,holes,output,provenance));return reports
