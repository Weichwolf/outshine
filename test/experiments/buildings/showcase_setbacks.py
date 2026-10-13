"""A connected three-storey house: source plan, retained circulation and nested terraces."""

import argparse
import hashlib
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from building_plan_views import patch
from building_plan import wall_at
from building_setbacks import apply
from building_vertical import build,mesh
from evidence import revision
from preview import configuration
from showcase_groups import save_plot
from showcase_profile import PROFILE
from showcase_scene import fitted


def house():
    return apply(build(dict(id='setback-house',polygon=shapely.box(0,0,18,14),
        properties={'render_height':9.,'building:levels':3})),[0.,.8,1.6])


def probe(output,provenance,render):
    output.mkdir(parents=True,exist_ok=True);start=time.perf_counter();plan=house();product=mesh(plan)
    path=output/'setback-house-mesh.npz';np.savez_compressed(path,**product)
    fig,axes=plt.subplots(1,3,figsize=(19.2,10.8));fig.suptitle('Staffelhaus · gleicher Treppenkern, geschossweise Rücksprünge',fontsize=22)
    for index,(axis,current) in enumerate(zip(axes,plan['storey_plans'],strict=True)):
        patch(axis,plan['footprint'],facecolor='#edf0f3',edgecolor='#bbc3ca')
        patch(axis,current['terrace'],facecolor='#e9c788',edgecolor='#a57529')
        patch(axis,wall_at(current,current['floor_z_m']+.4),facecolor='#39454f',edgecolor='none')
        for room,kind in zip(current['rooms'],current['room_kinds'],strict=True):
            patch(axis,room,facecolor='#b0d3c6' if kind=='corridor' else '#ceaa7c' if kind=='stairwell' else '#d6e2e8',edgecolor='none')
        axis.set(xlim=(-1,19),ylim=(-1,15),aspect='equal',xlabel='Ost [m]',ylabel='Nord [m]',
            title=f'Geschoss {index+1} · {current["footprint"].area:.1f} m²')
        axis.grid(alpha=.15)
    fig.text(.06,.12,'Grün: gemeinsamer Flur. Braun: unveränderter Treppenkern. Ocker: neu freigelegte Terrassenfläche.',fontsize=14)
    fig.text(.06,.075,'Räume und Zugänge bleiben verbunden. Terrassentüren, Geländer und Fassadendetails folgen; keine fertige Rezeptfamilie.',fontsize=13)
    rows=[save_plot(fig,output,'current-setback-house-plan','Staffelhaus · Geschossgrundrisse',
        'Drei unterschiedliche Geschossflächen bei festem Treppenkern und gemeinsamen Fluren. Eigene Ergänzung auf einem synthetischen 18×14-m-Grundriss.',provenance,start,PROFILE,label='setback-house')]
    for opposite in (False,True):
        view='orbit' if opposite else 'near';stem='current-setback-house-'+view
        camera=fitted(product,PROFILE,opposite=opposite);config,_=configuration([],camera,False,output/(stem+'.png'))
        config['constructive']=dict(mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),wall_linear=[.72,.70,.65],roof_linear=[.24,.27,.29],extra_wall_colours=[])
        rows.append(render(config,output,stem,provenance|dict(page='buildings',view=view,label='setback-house',
            title='Staffelhaus · '+('Gegenansicht' if opposite else 'Gesamtansicht'),
            caption='Vollständiger Körper mit 3 Geschossen, 18 Raumflächen und 2 Treppenverbindungen. Rücksprünge 0/0,8/1,6 m; Flur und Kern erhalten. Terrassenzugänge/Geländer und Fassadendetails fehlen; Workbench-Geometriekontrolle.',
            triangles=len(product['faces']),storeys=3,rooms=18,setbacks=plan['setback_decisions'])))
    return rows


if __name__=='__main__':
    from showcase_snapshot import render
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/setback-development'))
    args=parser.parse_args();probe(args.output,revision(),render)
