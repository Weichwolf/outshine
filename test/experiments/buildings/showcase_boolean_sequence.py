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


def comparison_plot(output,provenance,path):
    record=json.loads(path.read_text())
    if record['status']!='pass' or record['commit']!=provenance['commit'] or record['experiment_tree_dirty']:
        raise ValueError('Cost plot requires passing comparison from the current clean source commit')
    started=time.perf_counter();fig,axes=plt.subplots(1,2,figsize=(32,18))
    names=('union-first','subtract-sequential');labels=('Außenkörper − (Raum ∪ Tür)','(Außenkörper − Raum) − Tür')
    samples=[[row['pipeline_ms'] for row in record['runs'] if row['strategy']==name] for name in names]
    triangles=[next(row['triangles'] for row in record['runs'] if row['strategy']==name) for name in names]
    for index,values in enumerate(samples):
        axes[0].bar(index,np.median(values),color=('#6b9488','#bfa36b')[index],width=.55)
        axes[0].scatter(index+np.linspace(-.1,.1,len(values)),values,color='#293d47',s=130,zorder=3)
        axes[0].text(index,max(values)+35,f'{np.median(values):.0f} ms Median',ha='center',fontsize=21)
    axes[0].set(xticks=[0,1],xticklabels=labels,ylabel='Erzeugungszeit [ms]',title='Zwei vollständige Operationen · drei Messungen je Weg')
    axes[0].set_ylim(0,max(map(max,samples))*1.2)
    axes[1].bar([0,1],triangles,color=['#6b9488','#bfa36b'],width=.55)
    axes[1].set(xticks=[0,1],xticklabels=labels,ylabel='Native Dreiecke',title='Gleiche Oberfläche · gleiche 16 logische Flächen')
    for index,value in enumerate(triangles):axes[1].text(index,value+20,str(value),ha='center',fontsize=24)
    axes[1].set_ylim(0,max(triangles)*1.2)
    for axis in axes:axis.tick_params(labelsize=18);axis.grid(axis='y',alpha=.15);axis.set_axisbelow(True)
    fig.suptitle('Gleiche Raum-/Türgeometrie · beide Erzeugungswege bestehen die Qualitätsprüfung',fontsize=28)
    fig.text(.035,.08,'378,4 m³ · 579,2 m² · ursprüngliche UV0–9, Materialien, Quellflächen und Mesh-Schnitte geprüft.\n'
        f'Prozess-Peak: {record["process_peak_rss_mib"]:.1f} MiB für den gesamten Vergleich einschließlich Prüfungen; keine Zuordnung zu einem Weg.\n'
        'Zeit ohne unabhängige Prüfung. Ein fester Lastfall auf dieser Maschine; kein allgemeiner Sieger und kein Frame-/Gerätenachweis.',fontsize=19)
    fig.tight_layout(rect=(0.,.19,1.,.93))
    name=save_plot(fig,output,'current-sequence-cost','Raum-/Türkette · Kosten bei gleicher Qualität',
        'Sechs abwechselnde Messungen zweier vollständiger CSG-/Shape-Ketten. Beide bestehen dieselben geometrischen und semantischen Prüfungen. '
        'Punkte zeigen Einzelmessungen, Balken deren Median; der Speicherpeak gehört zum gesamten Vergleichsprozess.',
        provenance,started,[3200,1800],label='semantic-shape')
    path=output/name;report=json.loads(path.read_text());report.update(comparison=record)
    path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');return name


def render(output,comparison):
    output.mkdir(parents=True,exist_ok=True);provenance=revision()
    _,_,_,together,_=room()
    _,_,cutter,shell,sections=room('subtract-sequential');shape=shell.operand.shape;reports=[]
    caption='Raum und Türöffnung werden nacheinander aus dem Außenkörper ausgeschnitten. Auch der Weg mit vereinigten Cuttern besteht dieselben Prüfungen. '
    caption+='Originale Blatt-/Face-/Triangle-Herkunft und UV0–9 bleiben erhalten; aktuelle bool.A/B beziehen sich auf den letzten Schritt. '
    caption+='Der erzeugte Wandkörper lässt einen 0,6 m breiten Weg durch den 2×3 m Eingang frei; native Navigation und vollständige Rezeptfamilien bleiben offen.'
    reports.append(render_scene(output,'current-sequence-shell','Verkettete CSG · Raum mit Türöffnung',caption,
        [shape],shape,provenance))
    view=View(np.array([6.,-3.,1.6]),np.array([6.,4.,1.6]),width=1920,height=1080)
    reports.append(render_scene(output,'current-sequence-entry','Innenraum · Blick durch die erzeugte Türöffnung',caption,
        [shape],shape,provenance,camera=view,page='interiors'))
    started=time.perf_counter();fig,axes=plt.subplots(1,2,figsize=(32,18))
    fig.suptitle('Erzeugte Wandschnitte · zwei gleichwertige CSG-Wege',fontsize=28)
    for axis,height in zip(axes,(.3,2.),strict=True):
        wall=generated_section(shell,height)
        alternative=generated_section(together,height)
        if wall.symmetric_difference(alternative).area>1e-7:raise ValueError('Showcase room expressions changed wall section')
        polygons=list(wall.geoms) if hasattr(wall,'geoms') else [wall]
        for polygon in polygons:
            xy=np.asarray(polygon.exterior.coords);axis.fill(*xy.T,color='#74a090',edgecolor='#344454')
            for hole in polygon.interiors:
                xy=np.asarray(hole.coords);axis.fill(*xy.T,color='white',edgecolor='#344454')
        axis.plot([6.,6.],[-.5,4.],color='#a8203d',linewidth=5.,label='Freier Weg · 0,6 m Breite')
        axis.fill_betweenx([-.5,4.],5.7,6.3,color='#a8203d',alpha=.2)
        axis.set(aspect='equal',title=f'Tatsächlicher Mesh-Schnitt bei {height:g} m',xlabel='Ost [m]',ylabel='Nord [m]')
        axis.tick_params(labelsize=16);axis.legend(fontsize=14);axis.grid(alpha=.15)
    fig.text(.035,.055,'Gezeigt: (Außenkörper − Raum) − Tür. Gleiche Schnitte wie Außenkörper − (Raum ∪ Tür).\n'
        'Quellen: outer / room / door. Volumen 378,4 m³; Oberfläche 579,2 m². Unabhängig gegen Originalprismen geprüft.',fontsize=18)
    fig.tight_layout(rect=(0.,.1,1.,.94))
    reports.append(save_plot(fig,output,'current-sequence-plan','Raum-/Türkette · tatsächliche begehbare Querschnitte',
        'Schnitte stammen aus dem erzeugten Mesh. Der markierte Radius 0,3 m bleibt bei 0,3/1/2 m Höhe frei. '
        'Eine native Lauf-/Kollisionssimulation ist damit noch nicht abgenommen.',provenance,started,[3200,1800],label='semantic-shape'))
    for name in reports:
        path=output/name;record=json.loads(path.read_text())
        record.update(operation_ids=[cutter.identifier,shell.identifier],source_ids=[leaf.identifier for leaf in shell.leaves],
            stages=2,final_triangles=len(shell.origins),route_sections=sections,
            equivalent_union_triangles=len(together.origins),output_repair=shell.product.overlay.output_repair)
        path.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    reports.append(comparison_plot(output,provenance,comparison))
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/sequence-rounding-release'))
    parser.add_argument('--comparison',type=Path,default=Path('build/experiments/buildings/boolean-sequence-comparison.json'))
    args=parser.parse_args();print(render(args.output,args.comparison))
