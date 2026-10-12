"""Inspect imported hierarchy and original UV channels from owned file controls."""

from dataclasses import asdict
import json
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from modelling_asset_cases import fixtures
from modelling_insert import Asset
from modelling_scope import Scope
from modelling_shape import Shape
from modelling_scope_probe import engine_mesh
from showcase_groups import COLOURS,save_plot
from showcase_profile import PROFILE
from showcase_scene import fitted_points
from showcase_split import render_control


def original(asset):
    return Shape(asset.geometry,Scope(position=asset.minimum,size=asset.geometry.reference_size),asset=asset)


def uv_plan(output,provenance,assets):
    start=time.perf_counter();fig,axes=plt.subplots(1,3,figsize=(15,7))
    for ax,(asset,channel,label) in zip(axes,((assets[0],'uv','OBJ · UV0'),(assets[1],'uv','glTF · UV0'),(assets[1],'uv1','glTF · UV1')),strict=True):
        mesh=original(asset).emit()
        for group in np.unique(mesh['groups']):
            rows=mesh['faces'][mesh['groups']==group]
            for row in rows:ax.plot(*np.r_[mesh[channel][row],mesh[channel][row[:1]]].T,color=COLOURS[int(group)%4],linewidth=1.5)
        ax.set_title(label+'\n'+asset.import_info.uv_convention);ax.set_aspect('equal');ax.set_xlim(0,1);ax.set_ylim(0,1)
        ax.set_xlabel('u');ax.set_ylabel('v');ax.grid(alpha=.2)
    title='Dateiimport · ursprüngliche UV-Kanäle erhalten'
    fig.suptitle(title+'\nStandarddecoder für Geometrie; Originalwerte unabhängig von Materialfarben und Hilfsnamen',fontsize=13)
    fig.subplots_adjust(left=.06,right=.97,bottom=.14,top=.8,wspace=.3)
    fig.text(.03,.025,'Texturkoordinaten in der jeweiligen Quellkonvention; Textur-Shading noch offen. '+provenance['commit'][:9],fontsize=10)
    return save_plot(fig,output,'current-import-uv-plan',title,
        'Die importierten Vertex-UVs, einschließlich UV1. Unterschiedliche OBJ-/glTF-Ursprünge bleiben ausdrücklich gekennzeichnet; noch kein Textur-Shading.',
        provenance,start,[1500,700],label='modelling-import')


def hierarchy_plan(output,provenance,asset):
    start=time.perf_counter();fig,axes=plt.subplots(1,2,figsize=(13,8));mesh=original(asset).emit()
    for ax,columns in zip(axes,((0,1),(0,2)),strict=True):
        for group,node in enumerate(asset.import_info.nodes):
            rows=mesh['faces'][mesh['groups']==group];points=mesh['vertices'][:,columns]
            for row in rows:ax.fill(*points[row].T,color=COLOURS[group%4],alpha=.4,edgecolor='#34404a')
            centre=points[np.unique(rows)].mean(0);ax.annotate(node[0],centre,fontsize=10)
        ax.set_aspect('equal');ax.grid(alpha=.2);ax.set_xlabel('Quell-x [m]');ax.set_ylabel('Quell-'+('y' if columns[1]==1 else 'z')+' [m]')
    title='Szenenimport · Elterntransformation und Spiegelung'
    fig.suptitle(title+'\nZwei Instanzen derselben Quellfläche; Knotenpfade, Rotation und nichtuniforme Skalierung erhalten',fontsize=13)
    fig.subplots_adjust(left=.09,right=.96,bottom=.12,top=.84,wspace=.3)
    fig.text(.03,.025,'Originale Weltdreiecke; glTF-Vorderseiten bleiben auch bei Spiegelung korrekt. '+provenance['commit'][:9],fontsize=10)
    return save_plot(fig,output,'current-import-hierarchy-plan',title,
        'Derselbe GLB-Kontrollimport in xy- und xz-Projektion. Die zweite Fläche ist gespiegelt; die Dreiecksorientierung folgt den transformierten Normalen.',
        provenance,start,[1300,800],label='modelling-import')


def render_import(output,provenance,key,title,asset):
    body=original(asset);children=body.comp('g')
    camera=fitted_points(np.concatenate([engine_mesh(child.emit())['vertices'] for child in children]),PROFILE,direction=(.3,-1.,.2))
    name=render_control(output,provenance,'import-'+key,title,children,camera);path=output/name;record=json.loads(path.read_text())
    record.update(label='modelling-import',caption='Eigene Datei als Asset importiert; Szenentransformationen, Quellmaterialwerte, UVs/Normalen und Gruppen erhalten. '
        'Opake Prüfpalette; Textur-/PBR-Shading und vollständige Architektur-Rezepte stehen noch aus.',
        geometry_scope='complete supported surface nodes of an owned file asset',import_source=asdict(asset.import_info),
        material_parameters=[dict(id=index,name=value.name,values=dict(value.attributes)) for index,value in asset.geometry.material_defs])
    path.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');return name


def probe(output,provenance):
    obj,paths,_=fixtures(output/'owned-import-files');assets=[Asset.from_file(obj),Asset.from_file(paths[0])]
    return [uv_plan(output,provenance,assets),hierarchy_plan(output,provenance,assets[1]),
            render_import(output,provenance,'obj','OBJ · negative Indices, Gruppen und Quellmaterial',assets[0]),
            render_import(output,provenance,'glb','GLB · zwei transformierte Quellinstanzen',assets[1])]
