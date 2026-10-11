"""Show actual constructed meshes and the vehicle paths they support."""

import argparse
import json
from pathlib import Path
from textwrap import fill

import numpy as np
import shapely

from infrastructure_building_clearance import plot_geometry
from infrastructure_cut_setback_experiment import digest


def render(report,path,bounds=None,pair=None):
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    for name,value in report['implementation_sha256'].items():
        assert digest(Path(name))==value,'numeric producer changed since the measured construction'
    assert digest(Path(report['physical_mesh']))==report['physical_mesh_sha256']
    before=shapely.from_wkb(bytes.fromhex(report['original_surface_wkb']))
    shape=shapely.from_wkb(bytes.fromhex(report['physical_shape_wkb']))
    planned=shapely.from_wkb(bytes.fromhex(report['planned_surface_wkb']))
    trials=[r for r in report['trials'] if pair is None or (r['entry'],r['exit'])==tuple(pair)]
    if pair is not None and not trials:raise ValueError('requested pair is absent from report')
    accepted=[r for r in trials if r['accepted']]
    unresolved=[r for r in trials if not r['accepted'] and r.get('source_path')]
    paths=[np.array(r['path']) for r in accepted]
    view=paths+[np.array(r['original_source_path']) for r in accepted if 'original_source_path' in r]
    view.extend(np.array(r['source_path']) for r in unresolved)
    a,b,c,d=shapely.MultiPoint(np.vstack(view)).bounds if view else shape.bounds
    if bounds is not None:
        a,b,c,d=bounds
        if not np.all(np.isfinite(bounds)) or c<=a or d<=b:
            raise ValueError('invalid roadbed view bounds')
    with np.load(report['physical_mesh']) as product:
        triangles=product['vertices'][product['indices']]
    visible=shapely.intersects(shapely.polygons(triangles),shapely.box(a-5,b-5,c+5,d+5))
    height=float(np.clip(6*(d-b+10)/(c-a+10)+1.5,2.5,8))
    fig,axes=plt.subplots(1,2,figsize=(12,height),constrained_layout=True)
    for index,ax in enumerate(axes):
        ax.set_facecolor('#f3eee1')
        if index==0:
            plot_geometry(ax,[shape],'#d3cbbc')
        else:
            ax.add_collection(PolyCollection(triangles[visible],facecolors='#d3cbbc',
                              edgecolors='#bbb3a8',linewidths=.2))
        plot_geometry(ax,[before if index==0 else planned],'#8d9993')
        if index:
            parts=shapely.get_parts(planned.difference(before))
            plot_geometry(ax,parts[np.isin(shapely.get_type_id(parts),[3,6])],'#71bd94')
        for row in unresolved:
            points=np.array(row['source_path'])
            ax.plot(points[:,0],points[:,1],'--',color='#b54a49',linewidth=1.2,zorder=4,
                    label='unresolved source route' if row is unresolved[0] else None)
            for body in row['endpoint_bodies']:
                outline=np.array(body)
                ax.fill(outline[:,0],outline[:,1],facecolor='#b54a49',alpha=.3)
        if unresolved:ax.legend(loc='best')
        for row,points in zip(accepted,paths):
            colour='#b54a49' if index==0 else '#286b9e'
            if index==0 and 'original_source_path' in row:
                points=np.array(row['original_source_path'])
                ax.plot(points[:,0],points[:,1],'--',color=colour,linewidth=2)
                for body in row['original_endpoint_bodies']:
                    body=np.array(body)
                    ax.fill(body[:,0],body[:,1],facecolor=colour,edgecolor=colour,alpha=.3)
                continue
            ax.plot(points[:,0],points[:,1],color=colour,linewidth=2)
            outline=np.array(row['diagnostic_body_outline'])
            outline=np.vstack((outline,outline[0]))
            ax.fill(outline[:,0],outline[:,1],facecolor=colour,edgecolor=colour,alpha=.3)
            middle=len(points)//2
            ax.annotate('',xy=points[middle+1],xytext=points[middle-1],
                        arrowprops=dict(arrowstyle='->',color=colour,lw=2))
            ax.plot(points[[0,-1],0],points[[0,-1],1],'o',color=colour,markersize=4)
        for section in report.get('original_sections' if index==0 else 'planned_sections',[]):
            line=np.array(section)
            ax.plot(line[:,0],line[:,1],color='#303c37',linewidth=1)
        ax.set_xlim(a-5,c+5)
        ax.set_ylim(b-5,d+5)
        ax.set_aspect('equal')
        ax.set_xlabel('east [m]')
        ax.set_ylabel('north [m]')
        count=report['previous_accepted'] if index==0 else report['accepted']
        ax.set_title(f'{"Original" if index==0 else "Constructed physical mesh"}: {count}/{report["connections"]} routes')
    cuts=report.get('shared_cuts_description','Fixed shared cuts')
    if pair is not None:cuts+=f'; displayed route {pair[0]} → {pair[1]}'
    caption=f'{cuts} · radius ≥ {report["vehicle"]["minimumRearAxleRadiusM"]:g} m · buildings/private space unchanged'
    fig.suptitle(f'{report["place"]} · module {report["module"]} · +{report["added_area_m2"]:.2f} m² roadbed\n'
                 f'{fill(caption,110)}')
    path.parent.mkdir(parents=True,exist_ok=True)
    fig.savefig(path,dpi=140)
    plt.close(fig)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,default=Path('build/experiments/infrastructure/roadbeds/roadbeds.json'))
    parser.add_argument('--output',type=Path,default=Path('build/shots/experiments/infrastructure/constructed-roadbed.png'))
    parser.add_argument('--bounds',type=float,nargs=4,metavar=('WEST','SOUTH','EAST','NORTH'))
    parser.add_argument('--pair',type=int,nargs=2,metavar=('ENTRY','EXIT'))
    args=parser.parse_args()
    render(json.loads(args.input.read_text()),args.output,args.bounds,args.pair)
    print(args.output)


if __name__=='__main__':
    main()
