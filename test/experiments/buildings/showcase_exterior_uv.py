"""Procedural checker texture sampled from actual native and exterior UV charts."""

import argparse
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from evidence import revision
from exterior_uv_probe import partition,values
from showcase_groups import save_plot


def example():
    corners=np.array([[[0.,0.],[6.,0.],[6.,2.5]],[[6.,0.],[12.,0.],[6.,2.5]],
                      [[12.,0.],[12.,5.],[6.,2.5]],[[12.,5.],[0.,5.],[6.,2.5]],
                      [[0.,5.],[0.,0.],[6.,2.5]]])
    uv=corners/1.5
    uv[1:3]+=[.4,.25]
    outside=shapely.box(-1.5,-1.5,13.5,0.)
    charts=partition(corners,np.array([0.,0.]),np.array([12.,0.]),outside)
    return corners,uv,charts


def raster(bounds,corners,uv,charts):
    west,south,east,north=bounds
    x=np.linspace(west,east,900);y=np.linspace(south,north,300)
    xx,yy=np.meshgrid(x,y);points=np.c_[xx.ravel(),yy.ravel()]
    colour=np.full((len(points),3),.94);owned=np.full(len(points),-1,dtype=int)
    for polygon,triangle in [(shapely.Polygon(c),i) for i,c in enumerate(corners)]+[(c.polygon,c.triangle) for c in charts]:
        mask=shapely.contains_xy(polygon,points[:,0],points[:,1])
        coordinates=values(corners,uv,triangle,points[mask])
        check=(np.floor(coordinates[:,0])+np.floor(coordinates[:,1])).astype(int)%2
        palette=np.array([[.12,.35,.55],[.80,.87,.91]])
        colour[mask]=palette[check];owned[mask]=triangle
    return colour.reshape(len(y),len(x),3),owned.reshape(len(y),len(x))


def render(output):
    output.mkdir(parents=True,exist_ok=True);started=time.perf_counter();provenance=revision()
    corners,uv,charts=example()
    fig,axes=plt.subplots(2,1,figsize=(38.4,21.6),gridspec_kw={'height_ratios':[1.,1.]})
    fig.suptitle('UV-Fortsetzung · native Trauf-Charts über den Grundriss hinaus',fontsize=32)
    for axis,bounds,title in zip(axes,[(-1.5,-1.5,13.5,2.5),(4.5,-1.5,7.5,1.5)],
                                ['Gesamte Traufkante · Außenbereich unter y=0','Nahansicht des Chartwechsels · 3×3 m'],strict=True):
        image,_=raster(bounds,corners,uv,charts)
        axis.imshow(image,extent=(bounds[0],bounds[2],bounds[1],bounds[3]),origin='lower',interpolation='nearest')
        axis.axhline(0.,color='#d89125',linewidth=4.,label='Ursprüngliche Traufkante')
        axis.axvline(6.,color='#bf3553',linewidth=3.,linestyle='--',label='Native Chartgrenze')
        axis.set(aspect='equal',xlabel='Entlang der Traufe [m]',ylabel='Innen / außen [m]',title=title)
        axis.tick_params(labelsize=18);axis.title.set_fontsize(24);axis.legend(fontsize=17,loc='upper right')
    fig.text(.035,.025,'Prozeduraler Checker aus tatsächlichen interpolierten UVs. Die Quelle besitzt eine UV-Naht bei x=6 m; '
             'beide Rand-Charts werden getrennt fortgesetzt. Keine SDK-UV- oder vollständige Überhang-Abnahme.',fontsize=21)
    fig.tight_layout(rect=(0.,.065,1.,.94),h_pad=3.)
    name=save_plot(fig,output,'current-exterior-uv-charts','Trauf-Charts · prozedurale UV-Fortsetzung',
                   'Native Dreiecke liefern Innen-UVs; außerhalb gilt der Chart des jeweiligen Traufsegments mit affiner Fortsetzung. '
                   'Chartwechsel und Eckbereiche bleiben explizit. Basis für positive Überhänge; deren gültige Gesamtgeometrie/Unterseite bleibt offen.',
                   provenance,started,[3840,2160],label='hip-exterior-uv')
    record=json.loads((output/name).read_text());record.update(source_triangles=len(corners),exterior_charts=len(charts),
                      geometry_scope='Explicit synthetic native-chart fixture, procedural checker; UV continuation only.')
    (output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
    print(name)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/exterior-uv-release'))
    render(parser.parse_args().output)
