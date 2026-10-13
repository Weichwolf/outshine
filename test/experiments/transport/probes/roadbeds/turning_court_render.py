"""Show a proved local transport court and unresolved building-clearance requests."""

import argparse
import json
from pathlib import Path
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from transport_building_clearance import plot_geometry


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--geometry',type=Path,required=True)
    parser.add_argument('--request',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();data=json.loads(args.geometry.read_text());request=json.loads(args.request.read_text())
    before,after,source=(shapely.from_wkb(data[k]) for k in ('original','planned','source'))
    buildings,conflict=(shapely.from_wkb(request[k]) for k in ('building_source_wkb','conflict_wkb'))
    route=np.array(data['path']);view=shapely.LineString(route).envelope.buffer(5).bounds
    fig,axes=plt.subplots(1,2,figsize=(13,7),layout='constrained')
    for ax,shape,title in zip(axes,(before,after),('Existing road: sharp turn unproved','Proved local turning court; building adaptation pending')):
        ax.set_facecolor('#e4ecdf');plot_geometry(ax,[buildings], '#adaba7');plot_geometry(ax,[shape], '#626970')
        if ax is axes[1]:
            plot_geometry(ax,[after.difference(before)],'#83b9bf');plot_geometry(ax,[conflict],'#e99469')
            ax.plot(route[:,0],route[:,1],color='#195ca5',linewidth=2,label=f'G2 driving curve, {shapely.LineString(route).length:.1f} m')
        xy=np.array(source.coords);ax.plot(xy[:,0],xy[:,1],'--',color='#ecc95c',linewidth=1.5,label='Source connection')
        ax.set(xlim=(view[0],view[2]),ylim=(view[1],view[3]),aspect='equal',title=title,xlabel='Local x [m]',ylabel='Local y [m]')
        ax.legend(loc='lower right',fontsize=8)
    fig.suptitle('Basel service-road fork: full vehicle on triangle mesh; shared cuts and building integration still open')
    args.output.parent.mkdir(parents=True,exist_ok=True);fig.savefig(args.output,dpi=140);plt.close(fig)


if __name__=='__main__':main()
