"""Two-dimensional height/UV evidence and spatial ridge-roof controls with unchanged footprints."""

import argparse
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from building_plan_views import patch
from evidence import revision
from esri_oracle import CASES
from modelling_components import Branch
from modelling_scope_probe import engine_mesh
from modelling_selectors import tagged
from modelling_shape import Shape
from provider_contract import attributes
from samples import read_site
from showcase_groups import save_plot
from showcase_roof_shed import spatial
from verify_modelling_roof_ridge import reframe
from verify_modelling_roof_shed import source


def top_mesh(roof):
    mesh=engine_mesh(roof.emit());selected=np.asarray(['roof.top' in f.tags for f in roof.geometry.polygons])[mesh['parts']]
    return mesh|dict(faces=mesh['faces'][selected])


def controls():
    polygon=shapely.Polygon([(0,0),(10,0),(10,3),(5,3),(5,8),(0,8)])
    return [(angle,polygon,reframe(Shape.from_polygon(polygon,3.),angle).roof_ridge(2.5,'byHeight')) for angle in (0.,31.,90.)]


def height_plan(output,provenance,rows):
    start=time.perf_counter();fig,axes=plt.subplots(1,3,figsize=(38.4,21.6));fig.suptitle('roofRidge · gleicher Grundriss, drei Scope-X-Ausrichtungen',fontsize=34)
    for axis,(angle,polygon,roof) in zip(axes,rows,strict=True):
        mesh=top_mesh(roof);points=mesh['vertices'];colour=axis.tricontourf(points[:,0],points[:,1],mesh['faces'],points[:,2]-3.,levels=np.linspace(0.,2.5,257),cmap='viridis')
        fig.colorbar(colour,ax=axis,shrink=.45,label='Höhe über Traufe [m]');patch(axis,polygon,facecolor='none',edgecolor='#2d3943',linewidth=2.)
        vertices=roof.emit()['vertices'];counts={}
        for face in roof.geometry.polygons:
            if 'roof.top' not in face.tags:continue
            for ring in (face.outer,*face.holes):
                for a,b in zip(ring,ring[1:]+ring[:1]):counts.setdefault(tuple(sorted((a,b))),0);counts[tuple(sorted((a,b)))]+=1
        for (a,b),count in counts.items():
            if count==2:axis.plot(vertices[[a,b],0],-vertices[[a,b],2],color='#df423b',linewidth=3.)
        axis.set(aspect='equal',xlabel='Ost [m]',ylabel='Nord [m]');axis.grid(alpha=.2);axis.tick_params(labelsize=16)
        axis.set_title(f'Scope-X {angle:g}° · First 2,50 m\n{sum("roof.top" in f.tags for f in roof.geometry.polygons)} Dachkomponenten',fontsize=24)
    fig.text(.055,.095,'Dachhöhe aus den emittierten Dreiecken; rot: gemeinsame logische Firstkanten. Der Quellgrundriss wird nicht mit dem Scope gedreht.',fontsize=23)
    fig.subplots_adjust(top=.82,bottom=.18,wspace=.25)
    return save_plot(fig,output,'current-roof-ridge-plan','Firstdächer · Scope-X und tatsächliche Höhen',
        'Drei Ausrichtungen desselben unveränderten L-Grundrisses. Zwei geneigte Ebenen; konkave Schnitte können mehrere getrennte Dachkomponenten erzeugen. Metrische Höhenausgabe und wirkliche gemeinsame Firstkanten.',
        provenance,start,[3840,2160],label='roof-ridge')


def uv_plan(output,provenance):
    start=time.perf_counter();original=source(shapely.box(0,0,8,6));roof=original.roof_ridge(3.,'byHeight')
    before=engine_mesh(original.emit());after=top_mesh(roof);fig,axes=plt.subplots(2,2,figsize=(19.2,10.8))
    for row,component in enumerate((0,1)):
        values=before['uv'][np.unique(before['faces']),component];levels=np.linspace(values.min(),values.max(),257)
        for column,(label,mesh) in enumerate((('Quelle',before),('Firstdach · Rückprojektion',after))):
            axis=axes[row,column];vertices=mesh['vertices'];values=mesh['uv'][:,component]
            colour=axis.tricontourf(vertices[:,0],vertices[:,1],mesh['faces'],values,levels=levels,cmap='viridis')
            axis.triplot(vertices[:,0],vertices[:,1],mesh['faces'],color='#263b51',alpha=.4,linewidth=.8)
            fig.colorbar(colour,ax=axis,shrink=.7,ticks=np.linspace(levels[0],levels[-1],5));axis.set(aspect='equal',xlabel='Ost [m]',ylabel='Nord [m]');axis.set_title(label+' · '+('U' if component==0 else 'V'),fontsize=16)
    fig.suptitle('Nichtaffine UV0 · gleiche Flächeninterpolation trotz neuer Firstpunkte',fontsize=24)
    fig.text(.045,.02,'Aus den wirklichen nativen UV-Streams. Die Kontrollen prüfen UV0–9 an Ecken und Flächenpunkten; absichtlich geänderte UVs werden erkannt.',fontsize=14)
    fig.subplots_adjust(top=.88,bottom=.10,hspace=.28,wspace=.23)
    return save_plot(fig,output,'current-roof-ridge-uv','Firstschnitt · nichtaffine UV-Flächen erhalten',
        'Links ursprüngliche Dreiecksinterpolation, rechts zurückprojiziertes Firstdach. Neue Punkte interpolieren in den jeweiligen Quell-Dreiecken; die UV-Fläche wird nicht aus den Polygonrändern neu trianguliert.',
        provenance,start,[1920,1080],label='roof-ridge')


def mass(polygon,base,eave,roof):
    body=Shape.from_polygon(polygon,base).extrude(eave-base)
    return [(child,0) for child in body.comp('f',Branch(~tagged('extrude.top')),alignment='noAlign')]+[
        (child,3) for child in roof.comp('f',Branch(~tagged('roof.bottom')),alignment='noAlign')]


def probe(output,provenance):
    output.mkdir(parents=True,exist_ok=True);rows=controls();reports=[height_plan(output,provenance,rows),uv_plan(output,provenance)];shapes=[]
    for index,(_,polygon,roof) in enumerate(rows):
        shapes.extend((shape.translate('rel','world',index*14.,0.,0.),colour) for shape,colour in mass(polygon,0.,3.,roof))
    reports.append(spatial(output,provenance,'current-roof-ridge-controls','Firstdächer · gleiches L, drei Ausrichtungen',shapes,{},
        'Derselbe Quellgrundriss und dieselbe Gesamthöhe in allen drei Modellen. Die Scope-X-Ausrichtung ändert den First: 0°, 31°, 90°. Tatsächliche native Dreiecke, opake Prüfpalette; keine Architektur-Rezeptfamilie.',label='roof-ridge'))
    _,site,identifier=CASES[1];sample=next(row for row in read_site(site)[0] if row['id']==identifier);data=attributes(sample['properties']);rise=min(3.,data['bodyHeight']*.25);eave=data['buildingHeight']-rise
    roof=Shape.from_polygon(sample['polygon'],eave).roof_ridge(rise,'byHeight')
    metadata=dict(source_id=sample['id'],source_sha256=sample['source_sha256'],source_properties=sample['properties'],source_holes=len(sample['polygon'].interiors),
        provider_height_interval_m=[data['minHeight'],data['buildingHeight']],experimental_roof=dict(mode='byHeight',value=rise,eave_m=eave,ridge_direction=roof.roof_info.ridge_directions[0]))
    reports.append(spatial(output,provenance,'current-roof-ridge-vienna','Wien · Firstdach und offener Innenhof',mass(sample['polygon'],data['minHeight'],eave,roof),metadata,
        'Gepinnter Anbietergrundriss mit Innenhof und unverändertem Höhenintervall. Experimenteller First innerhalb der vorhandenen Gesamthöhe. Native Dachflächen und Hofwände; Fenster, Räume und vollständiges Rezept fehlen.',label='roof-ridge',direction=(1.,-1.3,2.)))
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/roof-ridge-release'))
    args=parser.parse_args();probe(args.output,revision())
