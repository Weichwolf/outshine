"""Render frozen driving requests, public floor, reservations and full port bodies."""

import argparse
import json
from pathlib import Path
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
import numpy as np
import shapely
from shapely.plotting import patch_from_polygon

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from transport_vehicle_sweep import body_outlines


def draw_request(ax,path):
    data=json.loads(path.read_text())
    floor,reserved,source=(shapely.from_wkb(data[k]) for k in ('surface','reservations','source_route'))
    vehicle=data['vehicle'];margin=vehicle['minimumRearAxleRadiusM']+vehicle['frontFromRearAxleM']
    a,b,c,d=source.bounds;bounds=shapely.box(a-margin,b-margin,c+margin,d+margin)
    for surface,color in ((floor,'#87968d'),(reserved.difference(floor),'#d9c5a3')):
        for polygon in shapely.get_parts(surface.intersection(bounds)):
            if polygon.geom_type=='Polygon' and not polygon.is_empty:
                ax.add_patch(patch_from_polygon(polygon,facecolor=color,edgecolor='none'))
    positions=np.array([data[k]['position'] for k in ('entry','exit')])
    directions=np.array([data[k]['heading'] for k in ('entry','exit')])
    headings=np.arctan2(directions[:,1],directions[:,0])
    bodies=shapely.polygons(body_outlines(positions,headings,vehicle['widthM']*.5,
                                        vehicle['frontFromRearAxleM'],vehicle['rearOverhangM']))
    for polygon in bodies:ax.add_patch(patch_from_polygon(polygon,facecolor='#377eb8',edgecolor='#205c88',alpha=.6))
    xy=shapely.get_coordinates(source);ax.plot(*xy.T,'--',color='#bd564b',linewidth=1.5)
    ax.quiver(*positions.T,*directions.T,angles='xy',scale_units='xy',scale=1/3,color='#17415e')
    for label,point in zip(('Entry','Exit'),positions):ax.annotate(label,point,xytext=(3,4),textcoords='offset points')
    ax.set(xlim=(a-margin,c+margin),ylim=(b-margin,d+margin),aspect='equal',
        xlabel='East [m]',ylabel='North [m]',title=f'{path.parent.name} / {path.stem.replace("-input","")}')
    ax.set_facecolor('#f3f0e7')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,nargs='+',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();columns=min(2,len(args.input));rows=(len(args.input)+columns-1)//columns
    figure,axes=plt.subplots(rows,columns,figsize=(7.5*columns,7*rows),squeeze=False)
    for ax,path in zip(axes.flat,args.input):draw_request(ax,path)
    for ax in list(axes.flat)[len(args.input):]:ax.set_visible(False)
    figure.legend(handles=[Patch(facecolor=color,label=label) for color,label in
        (('#87968d','Existing public motor floor'),('#d9c5a3','Reserved outside existing floor'),
         ('#377eb8','Full vehicle at fixed ports'))],loc='lower center',ncol=3)
    figure.suptitle('Frozen road requests · dashed line: source route · arrows: travel heading')
    figure.tight_layout(rect=(0,.04,1,.97));args.output.parent.mkdir(parents=True,exist_ok=True)
    figure.savefig(args.output,dpi=140);plt.close(figure)


if __name__=='__main__':main()
