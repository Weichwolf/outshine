"""Show a frozen route, its public floor, reservations and endpoint bodies."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from transport_building_clearance import plot_geometry
from transport_vehicle_sweep import body_outlines


def render(inputs,output):
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(1,len(inputs),figsize=(6*len(inputs),7),squeeze=False,constrained_layout=True)
    for ax,path in zip(axes[0],inputs):
        data=json.loads(path.read_text())
        floor,reserved,route=(shapely.from_wkb(data[k]) for k in ('surface','reservations','source_route'))
        points=shapely.get_coordinates(route)
        vehicle=data['vehicle']
        xy=np.array([data[k]['position'] for k in ('entry','exit')])
        headings=np.array([data[k]['heading'] for k in ('entry','exit')])
        bodies=body_outlines(xy,np.arctan2(headings[:,1],headings[:,0]),vehicle['widthM']*.5,
                            vehicle['frontFromRearAxleM'],vehicle['rearOverhangM'])
        plot_geometry(ax,[floor],'#8d9993')
        plot_geometry(ax,[reserved.difference(floor)],'#d9a76a')
        ax.plot(points[:,0],points[:,1],'--',color='#286b9e',linewidth=2)
        for index,body in enumerate(bodies):
            color='#3b8c67' if shapely.covers(floor,shapely.Polygon(body)) else '#b54a49'
            ax.fill(body[:,0],body[:,1],facecolor=color,edgecolor=color,alpha=.4)
            ax.annotate('entry' if index==0 else 'exit',xy=xy[index]+3*headings[index],xytext=xy[index],
                        arrowprops=dict(arrowstyle='->',color=color),color=color)
        a,b,c,d=route.bounds
        ax.set(xlim=(a-7,c+7),ylim=(b-7,d+7),xlabel='east [m]',ylabel='north [m]',
               title=f'{path.stem}\nsource length {route.length:.1f} m; minimum radius {vehicle["minimumRearAxleRadiusM"]:g} m')
        ax.set_facecolor('#f3eee1');ax.set_aspect('equal')
    output.parent.mkdir(parents=True,exist_ok=True)
    fig.savefig(output,dpi=140);plt.close(fig)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,nargs='+',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();render(args.input,args.output)
    print(args.output)


if __name__=='__main__':main()
