"""Metric shed-roof height plans and spatial source-footprint controls at 1080p or higher."""

import argparse
import hashlib
import json
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
from outshine_ref.buildings.modelling.components import Branch
from modelling_scope_probe import engine_mesh
from outshine_ref.buildings.modelling.selectors import tagged
from outshine_ref.buildings.modelling.shape import Shape
from preview import configuration,run_render
from provider_contract import attributes
from samples import read_site
from showcase_groups import COLOURS,inspection_mesh,save_plot
from showcase_scene import fitted_points


def controls():
    outlines=[('Rechteck',shapely.box(0,0,8,6),20.,1,'byAngle'),
        ('L-Form',shapely.Polygon([(0,0),(8,0),(8,3),(4,3),(4,7),(0,7)]),2.5,3,'byHeight'),
        ('Innenhof',shapely.Polygon([(0,0),(10,0),(10,8),(0,8)],holes=[[(2,2),(2,6),(7,6),(7,2)]]),-15.,0,'byAngle')]
    return [(name,polygon,Shape.from_polygon(polygon,3.).roof_shed(value,index,mode),value,index,mode) for name,polygon,value,index,mode in outlines]


def plan(output,provenance,rows):
    start=time.perf_counter();fig,axes=plt.subplots(1,3,figsize=(38.4,21.6));fig.suptitle('roofShed · tatsächliche Dachhöhen über dem Quellgrundriss',fontsize=34)
    for axis,(name,polygon,roof,value,index,mode) in zip(axes,rows,strict=True):
        top=roof.comp('f',Branch(tagged('roof.top')),alignment='noAlign')[0];mesh=engine_mesh(top.emit());points=mesh['vertices']
        colour=axis.tricontourf(points[:,0],points[:,1],mesh['faces'],points[:,2]-3.,levels=np.linspace(0.,roof.roof_info.rises_m[0],257),cmap='viridis')
        fig.colorbar(colour,ax=axis,shrink=.45,label='Höhe über Traufe [m]')
        patch(axis,polygon,facecolor='none',edgecolor='#263544',linewidth=2.)
        corners=np.array(polygon.exterior.coords);edge=int(round(index))%(len(corners)-1);a,b=corners[edge:edge+2]
        axis.annotate('',xy=b,xytext=a,arrowprops=dict(arrowstyle='->',color='#de4237',lw=4));axis.text(*(a+b)/2,' Kante '+str(edge),color='#bd2a21',fontsize=20)
        axis.set(aspect='equal',xlabel='Ost [m]',ylabel='Nord [m]');axis.grid(alpha=.2);axis.tick_params(labelsize=16)
        axis.set_title(f'{name} · {mode} {value:g}\nAnstieg {roof.roof_info.rises_m[0]:.3f} m',fontsize=24)
    fig.text(.06,.10,'Dreieckswerte aus Shape.roof_shed; rote Pfeile: gewählte Quellkanten. Innenhöfe bleiben offen; negative Winkel kehren die Dachrichtung um.',fontsize=23)
    fig.subplots_adjust(top=.82,bottom=.18,wspace=.25)
    return save_plot(fig,output,'current-roof-shed-plan','Pultdächer · Kantenwahl und metrische Dachhöhen',
        'Rechteck, konkave L-Form und Innenhof mit Winkel-/Höhensteuerung. Farben zeigen die wirklichen emittierten Dachhöhen, keine separate Illustration. Analytische Geometrie geprüft; isolierte CGA-Ausgabe und Trim-Planes fehlen.',
        provenance,start,[3840,2160],label='roof-shed')


def mass(polygon,base,eave,roof):
    body=Shape.from_polygon(polygon,base).extrude(eave-base)
    body_faces=tuple(f for f in body.geometry.polygons if 'extrude.top' not in f.tags)
    roof_faces=tuple(f for f in roof.geometry.polygons if 'roof.bottom' not in f.tags)
    return [(Shape.from_faces(body.emit()['vertices'],body_faces),0),(Shape.from_faces(roof.emit()['vertices'],roof_faces),3)]


def spatial(output,provenance,stem,title,shapes,metadata,caption,label='roof-shed',direction=(1.,-1.3,.9),camera=None):
    mesh=inspection_mesh(shapes);path=output/(stem+'-mesh.npz');np.savez_compressed(path,**mesh);image=output/(stem+'.png')
    camera=fitted_points(mesh['vertices'],[1920,1080],direction=direction) if camera is None else camera
    config,_=configuration([],camera,False,image)
    config.update(inspection_workbench=True,constructive=dict(mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        wall_linear=[1.,1.,1.],roof_linear=[.2,.2,.2],extra_wall_colours=COLOURS))
    rendered=run_render(config,output/(stem+'-scene.json'));name=stem+'-report.json'
    record=provenance|rendered|metadata|dict(representation='showcase-current',page='buildings',view='components',label=label,title=title,caption=caption,
        profile=[1920,1080],triangles=len(mesh['faces']),image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),completed_recipe_families=0,lod_status='native LOD unproved')
    (output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');return name


def probe(output,provenance):
    output.mkdir(parents=True,exist_ok=True);rows=controls();reports=[plan(output,provenance,rows)];shapes=[]
    for index,(_,polygon,roof,_,_,_) in enumerate(rows):
        shapes.extend((shape.translate('rel','world',index*14.,0.,0.),colour) for shape,colour in mass(polygon,0.,3.,roof))
    reports.append(spatial(output,provenance,'current-roof-shed-controls','Pultdächer · Rechteck, L-Form und Innenhof',shapes,{},
        'Dieselben drei Grundrisse und Parameter wie im Höhenplan. Blau: Quellkörper, Gelb: geneigte Dachflächen und Seiten. Gemeinsame inneren Kappen ausgelassen; opake Prüfpalette, noch keine Architektur-Rezeptfamilie.'))
    _,site,identifier=CASES[1];sample=next(row for row in read_site(site)[0] if row['id']==identifier);data=attributes(sample['properties'])
    rise=min(3.,data['bodyHeight']*.25);eave=data['buildingHeight']-rise
    roof=Shape.from_polygon(sample['polygon'],eave).roof_shed(rise,3,'byHeight')
    metadata=dict(source_id=sample['id'],source_sha256=sample['source_sha256'],source_properties=sample['properties'],source_holes=len(sample['polygon'].interiors),
        provider_height_interval_m=[data['minHeight'],data['buildingHeight']],experimental_roof=dict(mode='byHeight',value=rise,edge_index=3,eave_m=eave))
    reports.append(spatial(output,provenance,'current-roof-shed-vienna','Wien · Pultdach über echtem Hofgrundriss',mass(sample['polygon'],data['minHeight'],eave,roof),metadata,
        'Gepinnter Anbietergrundriss einschließlich Innenhof und geliefertem Höhenintervall. Experimentelle Dachwahl: Anstieg innerhalb der vorhandenen Gesamthöhe reserviert. Formkontrolle ohne Fenster, Raumplanung oder Anspruch auf ein vollständiges Rezept.'))
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/roof-shed-release'))
    args=parser.parse_args();probe(args.output,revision())
