"""Inspect asset insertion modes and shared hollow frames on an actual source facade."""

from dataclasses import replace
import json
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from modelling_insert import Asset,MODES
from modelling_insert_cases import block,frame
from outshine_ref.buildings.modelling.scope import Scope
from modelling_scope_probe import engine_mesh
from modelling_shape import Shape
from modelling_split_sizes import Part
from showcase_groups import COLOURS,save_plot
from showcase_profile import PROFILE
from showcase_scene import fitted_points
from showcase_split import render_control
from verify_modelling_groups import cube


def modes_plan(output,provenance):
    start=time.perf_counter();fig,axes=plt.subplots(1,3,figsize=(15,7));source=block();asset=Asset.from_shape(source,'off-origin')
    parent=replace(source,scope=Scope(size=(8.,6.,12.)))
    for ax,mode in zip(axes,MODES,strict=True):
        mesh=parent.insert(asset,mode).emit();points=mesh['vertices'][:,[0,2]]
        ax.fill([0,8,8,0],[0,0,12,12],facecolor='#e5e8e6',edgecolor='#89969a',label='Elternscope')
        low,high=points.min(0),points.max(0)
        ax.fill([low[0],high[0],high[0],low[0]],[low[1],low[1],high[1],high[1]],color=COLOURS[0],alpha=.65,label='eingesetztes Asset')
        ax.set_xlim(-3,11);ax.set_ylim(-2,15);ax.set_aspect('equal');ax.grid(alpha=.2);ax.set_title(mode,fontsize=10)
        ax.set_xlabel('Scope-x [m]');ax.set_ylabel('Scope-z [m]');ax.legend(fontsize=9)
    title='Bauteile einsetzen · drei Modi mit derselben Quellgeometrie'
    fig.suptitle(title+'\nQuelle: Ursprung (−2, 3, 1) m, Größe 4×2×8 m · Elternscope 8×6×12 m',fontsize=13)
    fig.subplots_adjust(left=.05,right=.97,bottom=.15,top=.8,wspace=.3)
    fig.text(.03,.03,'Geometrie und Eigenschaften werden gemeinsam genutzt; noch keine GPU-Instanzen. '+provenance['commit'][:9],fontsize=10)
    return save_plot(fig,output,'current-insert-modes-plan',title,
        'Links vollständig eingepasst; Mitte Originalmaß und ursprüngliche Position; rechts Originalmaß im xz-Zentrum und auf y=0. Tatsächliche ausgegebene Bounds.',
        provenance,start,[1500,700],label='modelling-insert')


def facade_cases():
    source=cube().comp('f')[2];asset=Asset.from_shape(frame(),'hollow-frame');children=[];parents=[]
    for column,bay in enumerate(source.split('x',Part(1.5),repeat=True)):
        for row,cell in enumerate(bay.split('y',Part(1.2),repeat=True)):
            scope=replace(cell.scope.t(.15,.12,0.),size=(abs(cell.scope.size[0])-.3,abs(cell.scope.size[1])-.24,0.))
            target=replace(cell,scope=scope);parents.append(target);children.append(target.insert(asset))
    return source,asset,tuple(parents),tuple(children)


def frame_plan(output,provenance,parents,children):
    start=time.perf_counter();fig,ax=plt.subplots(figsize=(12,8));origin=parents[0].scope.origin;axes=parents[0].scope.axes
    for i,child in enumerate(children):
        mesh=child.emit();xy=((mesh['vertices']-origin)@axes)[:,:2]
        for triangle in mesh['faces']:ax.fill(*xy[triangle].T,color=COLOURS[i%4],alpha=.65,edgecolor='#34404a',linewidth=.35)
    ax.set_aspect('equal');ax.grid(alpha=.2);ax.set_xlabel('Fassaden-x [m]');ax.set_ylabel('Fassaden-y [m]')
    title='Geteilte Rahmengeometrie · Öffnungen bleiben frei'
    fig.suptitle(title+'\n'+str(len(children))+' Platzierungen eines einzigen vorbereiteten Rahmens',fontsize=13)
    fig.subplots_adjust(left=.1,right=.96,bottom=.12,top=.85)
    fig.text(.03,.02,'Tatsächliche Meshdreiecke und leere Innenkonturen; keine Scheiben oder Innenräume ergänzt. '+provenance['commit'][:9],fontsize=10)
    return save_plot(fig,output,'current-insert-frame-plan',title,
        'Die gleiche hohle Quellgeometrie in unterschiedlichen Fassadenscopes. Maße folgen dem jeweiligen Scope; fehlende Tiefe folgt dem dokumentierten Proportionsmittel.',
        provenance,start,[1200,800],label='modelling-insert')


def render_insert(output,provenance,key,title,children,camera,asset):
    name=render_control(output,provenance,'insert-'+key,title,children,camera);path=output/name;record=json.loads(path.read_text())
    record.update(label='modelling-insert',caption='Vorbereitete Bauteilgeometrie wird zwischen allen Platzierungen geteilt; Aufwärtsachsen, Scopes und Quellattribute erhalten. '
        'Das Prüf-Rendering materialisiert die Instanzen; native GPU-Instanzen und vollständige Architektur-Rezepte stehen noch aus.',
        geometry_scope='shared prepared asset geometry with independent placement scopes',asset_name=asset.name,
        placements=len(children),unique_geometries=len({id(child.geometry) for child in children}),
        instance_scopes=[dict(position=child.scope.position,size=child.scope.size) for child in children])
    path.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');return name


def probe(output,provenance):
    source,asset,parents,children=facade_cases()
    points=np.concatenate([engine_mesh(child.emit())['vertices'] for child in children])
    direction=engine_mesh(dict(vertices=np.array([source.scope.axes[:,2]]),faces=np.empty((0,3),dtype=int)))['vertices'][0]
    camera=fitted_points(points,PROFILE,direction=direction+np.array([.15,0.,.15]))
    reports=[modes_plan(output,provenance),frame_plan(output,provenance,parents,children),
             render_insert(output,provenance,'facade','Fassade · wiederverwendete räumliche Rahmen',children,camera,asset)]
    source=block();asset=Asset.from_shape(source,'off-origin')
    parents=[replace(source,scope=Scope(position=(i*10.,0.,0.),size=(8.,6.,12.))) for i in range(3)]
    children=tuple(parent.insert(asset,mode) for parent,mode in zip(parents,MODES,strict=True))
    camera=fitted_points(np.concatenate([engine_mesh(child.emit())['vertices'] for child in children]),PROFILE)
    return reports+[render_insert(output,provenance,'modes','Bauteil · drei räumliche Einfügemodi',children,camera,asset)]
