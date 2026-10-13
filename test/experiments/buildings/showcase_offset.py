"""Metric offset cases showing preserved components and signed border orientation."""

import argparse
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import shapely

from building_plan_views import patch
from evidence import revision
from modelling_offset_plan import offset
from showcase_groups import save_plot
from verify_offset_plan import cases


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
        'Negative und positive Distanzen, Innenhof, spitzer Winkel, Zerfall und Kollabieren. Fünf inward-Vergleiche mit dem gepinnten Esri-SDK-Helfer; dieser liefert nur die größte Teilfläche. Shape-/UV-/Tag-Anbindung und CGA-Okklusionsabfragen offen.',
        provenance,start,[3840,2160],label='offset-plan')]


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/offset-release'))
    args=parser.parse_args();probe(args.output,revision())
