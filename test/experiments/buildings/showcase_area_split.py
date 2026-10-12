"""Inspect native area partitions in metric plans, courtyard roofs and a bent surface."""

import json
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import numpy as np

from esri_oracle import CASES
from modelling_area_split_probe import surfaces
from modelling_scope_probe import engine_mesh
from modelling_shape import Shape
from modelling_split_sizes import Floating,Part
from provider_contract import attributes
from samples import read_site
from showcase_groups import COLOURS,save_plot
from showcase_profile import PROFILE
from showcase_scene import fitted_points
from showcase_split import render_control
from verify_modelling_area_split import area


def pattern(weights):
    return tuple(Part(Floating(weight),str(index)) for index,weight in enumerate(weights))


def plan(output,provenance,roof,facade):
    start=time.perf_counter();fig,axes=plt.subplots(2,2,figsize=(13,11))
    for row,(source,coordinates,label) in enumerate(((roof,(0,2),'Wien · Hofgrundriss'),(facade,(0,1),'Dreieckige Fassade'))):
        for column,operation in enumerate((source.split,source.split_area)):
            children=operation('x',*pattern((1,2,1)));ax=axes[row,column]
            for i,child in enumerate(children):
                mesh=child.emit();xy=((mesh['vertices']-source.scope.origin)@source.scope.axes)[:,coordinates]
                for triangle in mesh['faces']:ax.fill(*xy[triangle].T,color=COLOURS[i],alpha=.7,edgecolor='#34404a',linewidth=.6)
            values=' / '.join(f'{area(child):.2f}' for child in children)
            ax.set_title(label+' · '+('Breiten 1:2:1' if column==0 else 'Flächen 1:2:1')+'\n'+values+' m²')
            ax.set_aspect('equal');ax.grid(alpha=.15);ax.set_xlabel('Scope-x [m]');ax.set_ylabel('Scope-'+('z' if row==0 else 'y')+' [m]')
    title='Native Flächenteilung · Breite und Fläche sind unterschiedliche Vorgaben'
    fig.suptitle(title+'\nTatsächliche Schnittdreiecke; Höfe, UVs und Quellflächen bleiben erhalten',fontsize=13)
    fig.subplots_adjust(left=.08,right=.96,bottom=.09,top=.89,hspace=.35,wspace=.3)
    fig.text(.03,.02,'Eigener Modellierkern, noch keine Architektur-Rezeptfamilie. '+provenance['commit'][:9],fontsize=10)
    return save_plot(fig,output,'current-area-metric-plan',title,
        'Identische Quellen und Floating-Gewichte 1:2:1. Links verteilen sie Breiten, rechts tatsächliche 3D-Fläche. Dreiecke erhalten Hoflöcher und Fassadenränder.',
        provenance,start,[1300,1100],label='modelling-area')


def bent_plot(output,provenance,source):
    start=time.perf_counter();children=source.split_area('x',*pattern((1,1,1,1)))
    fig=plt.figure(figsize=(13,8));ax=fig.add_subplot(111,projection='3d');points=[]
    for i,child in enumerate(children):
        mesh=engine_mesh(child.t(i*.35,0,0).emit());points.append(mesh['vertices'])
        ax.add_collection3d(Poly3DCollection(mesh['vertices'][mesh['faces']],facecolors=COLOURS[i],edgecolors='#34404a',linewidths=1.,alpha=.75))
    points=np.concatenate(points);low,high=points.min(axis=0),points.max(axis=0)
    ax.set_xlim(low[0],high[0]);ax.set_ylim(low[1],high[1]);ax.set_zlim(low[2],high[2]);ax.set_box_aspect(high-low)
    ax.set_xlabel('Ost [m]');ax.set_ylabel('Nord [m]');ax.set_zlabel('Höhe [m]');ax.view_init(elev=24,azim=-58)
    title='Annähernd ebene Fläche · gleiche tatsächliche Flächen ohne Abflachen'
    values=' / '.join(f'{area(child):.4f}' for child in children)
    fig.suptitle(title+'\n'+values+' m² · vier erzeugte Teilflächen zur Kontrolle auseinandergezogen',fontsize=13)
    fig.subplots_adjust(left=.04,right=.96,bottom=.09,top=.86)
    fig.text(.03,.02,'Quelle steigt bis 0,6 m; Originaldreiecke und interpolierte Eigenschaften erhalten. '+provenance['commit'][:9],fontsize=10)
    return save_plot(fig,output,'current-area-bent-plan',title,
        'Die leicht verwundene Quelle bleibt räumlich erhalten. Eigene exakte Dreiecksflächen; SDK nutzt eine 2D-Projektion, deren Abweichung noch kein Laufzeitorakel belegt.',
        provenance|dict(source_area_m2=area(source),part_areas_m2=[area(child) for child in children]),start,[1300,800],label='modelling-area')


def render_area(output,provenance,key,title,source,weights,sample=None):
    children=source.split_area('x',*pattern(weights));exploded=tuple(child.t(i*.6,0,0) for i,child in enumerate(children))
    camera=fitted_points(np.concatenate([engine_mesh(child.emit())['vertices'] for child in exploded]),PROFILE,
                         direction=(.3,-.4,1.) if sample else (.15,-1.,.15))
    name=render_control(output,provenance,'area-'+key,title,exploded,camera,sample)
    path=output/name;record=json.loads(path.read_text())
    record.update(label='modelling-area',caption='Räumliche offene Quellfläche nach tatsächlichen Flächenanteilen geteilt; Teilstücke zur Kontrolle auseinandergezogen. '
        'Höfe, Material/Gruppen/Tags und Corner-UVs erhalten; keine künstlichen Abschlussflächen. Opake Prüfpalette, noch kein Architektur-Rezept.',
        geometry_scope='area-targeted cuts of complete open source surfaces without closure geometry',source_area_m2=area(source),
        part_areas_m2=[area(child) for child in children],area_weights=list(weights))
    path.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');return name


def probe(output,provenance):
    _,site,identifier=CASES[1];sample=next(row for row in read_site(site)[0] if row['id']==identifier);data=attributes(sample['properties'])
    roof=Shape.from_polygon(sample['polygon'],data['minHeight']+data['bodyHeight'],material=7,tags=(identifier,))
    cases={label:source for label,source,_,_ in surfaces()};facade=cases['triangular-facade']
    return [plan(output,provenance,roof,facade),bent_plot(output,provenance,cases['almost-planar']),
            render_area(output,provenance,'roof','Wien · Dachfläche in vier gleiche Flächen',roof,(1,1,1,1),sample),
            render_area(output,provenance,'facade','Dreieckige Fassade · Flächenanteile 1:2:1',facade,(1,2,1))]
