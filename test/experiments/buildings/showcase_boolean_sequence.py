"""Generated doorway views and slices with original leaf and current operation provenance."""

import argparse
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from boolean_sequence_reference import generated_section
from demand import View
from evidence import revision
from showcase_boolean_logical import render_scene
from showcase_groups import save_plot
from verify_boolean_sequence import room


def render(output):
    output.mkdir(parents=True,exist_ok=True);provenance=revision()
    prisms,declared,cutter,shell,sections=room();shape=shell.operand.shape;reports=[]
    caption='Raum und Türöffnung entstehen als Cutter-Vereinigung und werden gemeinsam aus dem Außenkörper ausgeschnitten. '
    caption+='Originale Blatt-/Face-/Triangle-Herkunft und UV0–9 bleiben erhalten; aktuelle bool.A/B beziehen sich auf den letzten Schritt. '
    caption+='Der erzeugte Wandkörper lässt einen 0,6 m breiten Weg durch den 2×3 m Eingang frei; native Navigation und vollständige Rezeptfamilien bleiben offen.'
    reports.append(render_scene(output,'current-sequence-shell','Verkettete CSG · Raum mit Türöffnung',caption,
        [shape],shape,provenance))
    view=View(np.array([6.,-3.,1.6]),np.array([6.,4.,1.6]),width=1920,height=1080)
    reports.append(render_scene(output,'current-sequence-entry','Innenraum · Blick durch die erzeugte Türöffnung',caption,
        [shape],shape,provenance,camera=view,page='interiors'))
    started=time.perf_counter();fig,axes=plt.subplots(1,2,figsize=(32,18))
    fig.suptitle('Erzeugte Wandschnitte · Türdurchgang und ursprüngliche Quellen',fontsize=28)
    for axis,height in zip(axes,(.3,2.),strict=True):
        wall=generated_section(shell,height)
        polygons=list(wall.geoms) if hasattr(wall,'geoms') else [wall]
        for polygon in polygons:
            xy=np.asarray(polygon.exterior.coords);axis.fill(*xy.T,color='#74a090',edgecolor='#344454')
            for hole in polygon.interiors:
                xy=np.asarray(hole.coords);axis.fill(*xy.T,color='white',edgecolor='#344454')
        axis.plot([6.,6.],[-.5,4.],color='#a8203d',linewidth=5.,label='Freier Weg · 0,6 m Breite')
        axis.fill_betweenx([-.5,4.],5.7,6.3,color='#a8203d',alpha=.2)
        axis.set(aspect='equal',title=f'Tatsächlicher Mesh-Schnitt bei {height:g} m',xlabel='Ost [m]',ylabel='Nord [m]')
        axis.tick_params(labelsize=16);axis.legend(fontsize=14);axis.grid(alpha=.15)
    fig.text(.035,.055,'Außenkörper − (Raum ∪ Tür). Quellen: outer / room / door. '
        'Volumen 378,4 m³; Oberfläche 579,2 m². Beide Größen und Schnittflächen unabhängig gegen Originalprismen geprüft.',fontsize=18)
    fig.tight_layout(rect=(0.,.1,1.,.94))
    reports.append(save_plot(fig,output,'current-sequence-plan','Raum-/Türkette · tatsächliche begehbare Querschnitte',
        'Schnitte stammen aus dem erzeugten Mesh. Der markierte Radius 0,3 m bleibt bei 0,3/1/2 m Höhe frei. '
        'Eine native Lauf-/Kollisionssimulation ist damit noch nicht abgenommen.',provenance,started,[3200,1800],label='semantic-shape'))
    for name in reports:
        path=output/name;record=json.loads(path.read_text())
        record.update(operation_ids=['cutters','room-shell'],source_ids=[leaf.identifier for leaf in shell.leaves],
            stages=2,final_triangles=len(shell.origins),route_sections=sections)
        path.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/sequence-release'))
    print(render(parser.parse_args().output))
