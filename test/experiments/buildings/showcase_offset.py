"""Metric offset cases showing preserved components and signed border orientation."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import shapely
import numpy as np

from building_plan_views import patch
from evidence import revision
from outshine_ref.buildings.modelling.offset_plan import offset
from outshine_ref.buildings.modelling.shape import Shape
from preview import configuration,run_render
from showcase_groups import save_plot,inspection_mesh,COLOURS
from showcase_scene import fitted_points
from verify_offset_plan import cases


def spatial(output,provenance):
    outlines=[shapely.box(0,0,6,4),shapely.Polygon([(0,0),(6,0),(6,2),(3,2),(3,5),(0,5)]),
        shapely.Polygon([(0,0),(8,0),(8,6),(0,6)],holes=[[(2,2),(2,4),(6,4),(6,2)]])]
    shapes=[]
    for index,polygon in enumerate(outlines):
        source=Shape.from_polygon(polygon).r(-90,0,0).t(index*10.,0,0)
        for kind,depth,colour in [('border',.25,3),('inside',.04,0)]:
            shapes.append((source.offset(-.3,kind).extrude(depth),colour))
    product=inspection_mesh(shapes);stem='current-offset-spatial';path=output/(stem+'-mesh.npz')
    np.savez_compressed(path,**product);image=output/(stem+'.png')
    camera=fitted_points(product['vertices'],[1920,1080],direction=(.35,1.,.55))
    config,_=configuration([],camera,False,image)
    config.update(inspection_workbench=True,constructive=dict(mesh=str(path.resolve()),
        mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),wall_linear=[1.,1.,1.],roof_linear=[.2,.2,.2],extra_wall_colours=COLOURS))
    rendered=run_render(config,output/(stem+'-scene.json'))
    name=stem+'-report.json';record=provenance|rendered|dict(representation='showcase-current',page='buildings',view='components',label='offset-plan',
        title='Offset → Extrusion · räumliche Rahmen',caption='Links Fläche mit Innenhof, Mitte L-Form, rechts Rechteck. 0,30 m Randbreite, 0,25 m Rahmentiefe und 0,04 m tiefe Füllung. '
            'Gemeinsam gedrehte und platzierte Quellflächen, danach Shape-Offset und Extrusion; opake Prüfpalette. Keine fertige Architekturfamilie oder volle CGA-Abnahme.',
        profile=[1920,1080],triangles=len(product['faces']),image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
        completed_recipe_families=0,lod_status='native LOD unproved')
    (output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');return name


def probe(output,provenance):
    output.mkdir(parents=True,exist_ok=True);start=time.perf_counter()
    rectangle=cases()[0][1];split=cases()[3][1];acute=cases()[2][1]
    courtyard=shapely.Polygon([(0,0),(20,0),(20,20),(0,20)],holes=[[(8,8),(8,12),(12,12),(12,8)]])
    controls=[('Rechteck innen',rectangle,-1.),('Rechteck außen',rectangle,1.),('Zwei erhaltene Teile',split,-.6),
        ('Innenhof',courtyard,-1.),('Spitzer Winkel',acute,.1),('Kollabierter Innenbereich',rectangle,-5.)]
    fig,axes=plt.subplots(2,3,figsize=(38.4,21.6));fig.suptitle('Offset · Flächenbilanz, Innenhöfe und Zerfall',fontsize=34)
    for axis,(name,polygon,distance) in zip(axes.flat,controls,strict=True):
        plan=offset(polygon,distance)
        for kind in ('inside','border'):
            for row in plan.regions:
                if row.kind==kind:patch(axis,row.polygon,facecolor='#6797d0' if kind=='inside' else '#ebc269',edgecolor='#485460',alpha=.7)
        patch(axis,polygon,facecolor='none',edgecolor='#162d40',linestyle='--',linewidth=2)
        extent=shapely.union_all([polygon]+[row.polygon for row in plan.regions]);x0,y0,x1,y1=extent.bounds
        margin=max(x1-x0,y1-y0)*.1
        count=sum(row.kind=='inside' for row in plan.regions)
        signed=sum(row.polygon.area*row.normal_sign for row in plan.regions)
        axis.set(xlim=(x0-margin,x1+margin),ylim=(y0-margin,y1+margin),aspect='equal',xlabel='X [m]',ylabel='Y [m]')
        axis.set_title(f'{name} · {distance:+g} m\n{count} Innenflächen · orientierte Summe {signed:.2f} m²',fontsize=23)
        axis.tick_params(labelsize=16);axis.grid(alpha=.2)
    fig.text(.08,.035,'Blau: Offsetflächen. Gelb: Randflächen; bei positivem Offset umgekehrte Normale. Gestrichelt: Quelle. Keine vollständige CGA-Abnahme.',fontsize=23)
    fig.subplots_adjust(top=.88,bottom=.12,hspace=.28,wspace=.18)
    return [save_plot(fig,output,'current-offset-plan','Offset · Innenflächen und orientierte Ränder',
        'Negative und positive Distanzen, Innenhof, spitzer Winkel, Zerfall und Kollabieren. Fünf inward-Vergleiche mit dem gepinnten Esri-SDK-Helfer; dieser liefert nur die größte Teilfläche. Shape-Anbindung mit affinen UV0–9, Tags und Materialien geprüft; vollständige CGA-Okklusionssemantik offen.',
        provenance,start,[3840,2160],label='offset-plan'),spatial(output,provenance)]


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/offset-release'))
    args=parser.parse_args();probe(args.output,revision())
