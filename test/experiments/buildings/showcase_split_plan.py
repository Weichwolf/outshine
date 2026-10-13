"""True 2D width and equal-area partitions of synthetic and provider footprints."""

import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import PathPatch
from matplotlib.path import Path
import numpy as np
import shapely

from modelling_extrude_probe import fixtures
from outshine_ref.buildings.modelling.plan_split import split_plan
from outshine_ref.buildings.modelling.split_sizes import Floating,Part
from showcase_groups import save_plot


COLOURS=['#28738a','#bc6b4f','#34926d','#c79935']


def patch(polygon,colour):
    polygon=shapely.orient_polygons(polygon);points=[];codes=[]
    for ring in (polygon.exterior,*polygon.interiors):
        coordinates=np.asarray(ring.coords);points.extend(coordinates)
        codes.extend([Path.MOVETO]+[Path.LINETO]*(len(coordinates)-2)+[Path.CLOSEPOLY])
    return PathPatch(Path(points,codes),facecolor=colour,edgecolor='#27333a',alpha=.7,lw=.8)


def probe(output,provenance):
    start=time.perf_counter();fig,axes=plt.subplots(5,2,figsize=(14,15))
    patterns=[Part(Floating(1.),str(i)) for i in range(4)];cases=list(fixtures());sources=[]
    for row,(label,polygon,_,_,source_sha) in enumerate(cases):
        sources.append(dict(label=label,source_sha256=source_sha,source_holes=len(polygon.interiors)))
        for column,area in enumerate((False,True)):
            ax=axes[row,column];children=split_plan(polygon,'x',patterns,area=area)
            for i,child in enumerate(children):
                for part in shapely.get_parts(child.geometry):
                    if not part.is_empty:ax.add_patch(patch(part,COLOURS[i]))
            lowx,lowy,highx,highy=polygon.bounds;margin=max(highx-lowx,highy-lowy)*.06
            ax.set_xlim(lowx-margin,highx+margin);ax.set_ylim(lowy-margin,highy+margin)
            ax.set_aspect('equal');ax.grid(alpha=.15);ax.set_xlabel('Quell-x [m]');ax.set_ylabel('Quell-y [m]')
            values=' / '.join(f'{child.geometry.area:.2f}' for child in children)
            ax.set_title(label+' · '+('gleiche Fläche' if area else 'gleiche Breite')+'\n'+values+' m²',fontsize=10)
    title='Grundrissteilung · gleiche Breite und gleiche Fläche'
    fig.suptitle(title+'\nEchte GEOS-Schnittpolygone; Höfe bleiben ausgespart, getrennte Teilflächen bleiben erhalten',fontsize=14)
    fig.subplots_adjust(left=.08,right=.97,bottom=.07,top=.90,hspace=.75,wspace=.25)
    fig.text(.03,.02,'2D-Verfahrensexperiment; noch keine räumlichen Mesh-/UV-Schnitte oder Architekturrezepte. '+provenance['commit'][:9],fontsize=10)
    return [save_plot(fig,output,'current-split-plan',title,
        'Fünf Grundrisse, jeweils vier tatsächliche Teilpolygone. Links gleiche x-Breiten, rechts je 25 % Quellfläche. '
        'Polygonlöcher sind ausgespart; keine vollständige CityEngine-split-Umsetzung.',provenance|dict(source_cases=sources),start,[1400,1500],label='modelling-split')]
