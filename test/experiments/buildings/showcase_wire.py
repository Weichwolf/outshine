"""Show current native line/point components and their local directions in metric views."""

import hashlib
import json
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from esri_oracle import CASES
from modelling_components import Branch
from modelling_selectors import tagged
from modelling_shape import Shape
from modelling_wire_components import WireSet
from provider_contract import attributes
from samples import read_site


def engine(points):return np.asarray(points)[...,[0,2,1]]*[1.,-1.,1.]


def configure(ax,points,title):
    low,high=points.min(axis=0),points.max(axis=0);centre=(low+high)/2;span=max(high-low)*.65+1.
    ax.set_xlim(centre[0]-span,centre[0]+span);ax.set_ylim(centre[1]-span,centre[1]+span);ax.set_zlim(centre[2]-span,centre[2]+span)
    ax.set_box_aspect((1,1,1));ax.view_init(elev=24,azim=-53)
    ax.set_xlabel('Ost [m]');ax.set_ylabel('Nord [m]');ax.set_zlabel('Höhe [m]');ax.set_title(title)


def draw(ax,source,kind,offset=.0):
    context=WireSet(source,kind);background=WireSet(source,'e');colours=plt.get_cmap('tab20').colors
    for record in background.records:
        points=engine(background.vertices[list(record.element.vertices)]);ax.plot(*points.T,color='#8b959c',alpha=.3,linewidth=.8)
    for index,record in enumerate(context.records):
        normal=engine(record.normal);points=engine(context.vertices[list(record.element.vertices)])+normal*offset;colour=colours[index%len(colours)]
        if kind in ('e','fe'):ax.plot(*points.T,color=colour,linewidth=2.)
        else:ax.scatter(*points[0],color=colour,s=20)
        centre=points.mean(axis=0);ax.quiver(*centre,*normal,length=.65,color=colour,arrow_length_ratio=.22)
        ax.text(*centre,str(index),fontsize=7,color=colour)
    return len(context.records)


def figure(source,output,key,title,kinds,provenance):
    start=time.perf_counter();fig=plt.figure(figsize=(15,10));counts={}
    for number,kind in enumerate(kinds,1):
        ax=fig.add_subplot(2,2,number,projection='3d');count=draw(ax,source,kind,.45 if kind in ('fe','fv') else 0.)
        configure(ax,engine(source.emit()['vertices']),kind+' · '+str(count)+' Komponenten');counts[kind]=count
    fig.suptitle(title+'\nPfeile: geometrische Normale · Zahlen: stabile Komponentenindizes · fe/fv um 0,45 m versetzt dargestellt',fontsize=13)
    fig.text(.02,.015,'Metrische Geometriekontrolle, keine neue Architektur-Rezeptfamilie oder LOD-Abnahme. '+provenance['commit'][:9],fontsize=9)
    fig.subplots_adjust(left=.04,right=.95,bottom=.08,top=.85,wspace=.12,hspace=.13)
    path=output/(key+'.png');fig.savefig(path,dpi=100);plt.close(fig)
    report=provenance|dict(representation='showcase-current',page='buildings',view='wire',label='modelling-wire',title=title,
        caption='Native Kanten und Punkte mit tatsächlichen Richtungen und Indizes. Graue Kanten zeigen den unveränderten Quellkörper; Versatz und Pfeile sind Prüfmarkierungen.',
        profile=[1500,1000],renderer='Metric Matplotlib 3D',render_seconds=time.perf_counter()-start,
        image_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),component_counts=counts,completed_recipe_families=0,
        lod_status='geometry inspection; native LOD unproved',measurement_scope='CPU metric plotting/PNG generation; no frame duration')
    name=key+'-report.json';(output/name).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    return name


def axes_figure(source,output,provenance):
    start=time.perf_counter();fig=plt.figure(figsize=(15,10));points=engine(source.emit()['vertices'])
    for number,kind in enumerate(('e','fe','v','fv'),1):
        ax=fig.add_subplot(2,2,number,projection='3d');shape=source.comp(kind)[0];origin=engine(shape.scope.pivot_origin)
        draw(ax,source,'e');axes=engine(shape.scope.axes.T)
        for colour,axis in zip(('red','green','blue'),axes,strict=True):ax.quiver(*origin,*axis,length=1.8,color=colour,linewidth=2.)
        configure(ax,points,kind+' · Scope '+', '.join(f'{size:g}' for size in shape.scope.size)+' m')
    fig.suptitle('Kanten-/Punkt-Pivots · erste Komponente jedes Typs\nRot: lokales x · Grün: lokales y · Blau: lokales z · metrische Weltlage erhalten',fontsize=13)
    fig.text(.02,.015,'Punkte haben Scope-Größe 0 × 0 × 0; einzelne Kanten nur Länge entlang x. '+provenance['commit'][:9],fontsize=9)
    fig.subplots_adjust(left=.04,right=.95,bottom=.08,top=.85,wspace=.12,hspace=.13)
    key='current-wire-scopes';path=output/(key+'.png');fig.savefig(path,dpi=100);plt.close(fig)
    report=provenance|dict(representation='showcase-current',page='buildings',view='plan',label='modelling-wire',title='Modellierkern · Kanten-/Punkt-Scopes',
        caption='Dieselben Quellkoordinaten, vier unterschiedliche lokale Richtungs-/Pivotverträge. Achsen sind explizite Prüfmarkierungen.',
        profile=[1500,1000],renderer='Metric Matplotlib 3D',render_seconds=time.perf_counter()-start,image_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        completed_recipe_families=0,lod_status='scope inspection; native LOD unproved',measurement_scope='CPU plotting/PNG generation; no frame duration')
    name=key+'-report.json';(output/name).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');return name


def plan_figure(source,output,provenance):
    start=time.perf_counter();fig,axes=plt.subplots(2,2,figsize=(15,10));counts={}
    colours=plt.get_cmap('tab20').colors
    bounds=engine(source.emit()['vertices'])[:,:2];low=bounds.min(axis=0);high=bounds.max(axis=0)
    margin=float(np.max(high-low))*.14
    for ax,kind in zip(axes.flat,('e','fe','v','fv'),strict=True):
        context=WireSet(source,kind);counts[kind]=len(context.records)
        for record in WireSet(source,'e').records:
            points=engine(context.vertices[list(record.element.vertices)])[:,:2]
            ax.plot(*points.T,color='#8b959c',alpha=.4,linewidth=1.)
        for index,record in enumerate(context.records):
            points=engine(context.vertices[list(record.element.vertices)])[:,:2]
            normal=engine(record.normal)[:2];centre=points.mean(axis=0);colour=colours[index%len(colours)]
            if kind in ('e','fe'):ax.plot(*points.T,color=colour,linewidth=2.)
            else:ax.scatter(*centre,color=colour,s=30)
            if np.linalg.norm(normal)>.01:
                ax.arrow(*centre,*(normal*1.2),color=colour,width=.025,head_width=.25,length_includes_head=True)
            ax.annotate(str(index),centre,xytext=(4,4),textcoords='offset points',fontsize=9,color=colour)
        ax.set_aspect('equal');ax.set_xlim(low[0]-margin,high[0]+margin);ax.set_ylim(low[1]-margin,high[1]+margin);ax.grid(alpha=.25)
        ax.set_xlabel('Ost [m]');ax.set_ylabel('Nord [m]');ax.set_title(kind+' · '+str(counts[kind])+' Komponenten')
    title='Wien · maßstäblicher Kanten-/Punktplan mit Innenhof'
    fig.suptitle(title+'\nPfeile: horizontale Normale · e/v zeigen senkrecht nach oben · Geometrie ohne Darstellungsversatz',fontsize=13)
    fig.text(.02,.015,'2D-Geometriekontrolle; Außen- und Hofringe bleiben getrennt. '+provenance['commit'][:9],fontsize=9)
    fig.subplots_adjust(left=.07,right=.97,bottom=.08,top=.86,wspace=.22,hspace=.25)
    key='current-wire-plan';path=output/(key+'.png');fig.savefig(path,dpi=100);plt.close(fig)
    report=provenance|dict(representation='showcase-current',page='buildings',view='plan',label='modelling-wire',title=title,
        caption='Echter Wiener Grundriss: Außenring und Innenhof. Flächenbezogene Kanten-/Punktnormalen zeigen vom Dachpolygon weg; eindeutige Kanten/Punkte haben die Dachnormale.',
        profile=[1500,1000],renderer='Metric Matplotlib 2D',render_seconds=time.perf_counter()-start,
        image_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),component_counts=counts,completed_recipe_families=0,
        lod_status='geometry inspection; native LOD unproved',measurement_scope='CPU metric plotting/PNG generation; no frame duration')
    name=key+'-report.json';(output/name).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');return name


def probe(output,provenance):
    source=Shape.from_polygon(shapely.box(0,0,8,5),2,tags=('control',)).extrude(4)
    reports=[figure(source,output,'current-wire-cube','Modellierkern · Kanten und Punkte am Kontrollvolumen',('e','fe','v','fv'),provenance),
             axes_figure(source,output,provenance)]
    _,site,identifier=CASES[1];sample=next(row for row in read_site(site)[0] if row['id']==identifier);data=attributes(sample['properties'])
    body=Shape.from_polygon(sample['polygon'],data['minHeight'],tags=(identifier,)).extrude(data['bodyHeight'],'world.up')
    top=body.comp('f',Branch(tagged('extrude.top')))[0]
    reports.append(figure(top,output,'current-wire-courtyard','Wien · Außenring und Hof als Kanten-/Punktkomponenten',('e','fe','v','fv'),provenance))
    reports.append(plan_figure(top,output,provenance))
    return reports
