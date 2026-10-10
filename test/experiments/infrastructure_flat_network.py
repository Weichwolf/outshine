#!/usr/bin/env python3
"""Run small single-level vector cases and real cached OSM windows."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import shapely

from infrastructure_flat_plan import plan,verify
from infrastructure_network_graph import traffic_of
from infrastructure_network import transport_widths
from infrastructure_travel import compile_travel


def render(path,place,source,accepted,graphs,view):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.collections import LineCollection
    fig,axes = plt.subplots(1,2,figsize=(14,7),constrained_layout=True)
    for ax,title in zip(axes,('Source axes; all levels','Planned axes; one flat level')):
        ax.set(xlim=(-view,view),ylim=(-view,view),aspect='equal',title=title,
               xlabel='East [m]',ylabel='North [m]',facecolor='#f1f0e8')
        ax.grid(alpha=.15)
    for role,color,style in (('ground','#879594','-'),('bridge','#167dba','-'),
                              ('tunnel','#a43896','--')):
        lines = [shapely.get_coordinates(r['line']) for r in source
                 if r['properties'].get('brunnel','ground')==role]
        axes[0].add_collection(LineCollection(lines,colors=color,linewidths=.6,linestyles=style,label=role))
    axes[0].legend()
    for tier,graph in graphs.items():
        axes[1].add_collection(LineCollection([shapely.get_coordinates(line) for line in graph['lines']],
                                colors='#bc7928' if tier[0]=='rail' else '#167dba',linewidths=.7))
        degree = np.bincount(graph['edges'].ravel(),minlength=len(graph['vertices']))
        points = graph['vertices'][degree>2]
        axes[1].scatter(*points.T,s=3,c='#394e39')
    fig.suptitle(f'{place}: vector topology experiment; junction surfaces and routing rules remain open')
    path.parent.mkdir(parents=True,exist_ok=True)
    fig.savefig(path,dpi=140)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--shots',type=Path,help='write only deliberately selected presentation images here')
    parser.add_argument('--view',type=float,default=600)
    parser.add_argument('places',nargs='+')
    args = parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    recipes = json.loads(Path(__file__).with_name('infrastructure_network_recipes.json').read_text())
    widths = transport_widths()
    small = verify(recipes)
    (args.output/'small-cases.json').write_text(json.dumps(small,indent=2)+'\n')
    for place in args.places:
        path = args.input/f'{place}-input.json'
        raw = json.loads(path.read_text())
        transport = [dict(line=shapely.LineString(r['coordinates']),width=r['width_m'],
                          properties=r['properties']) for r in raw['roads']]
        roads,graphs,report = plan(transport,recipes,widths)
        report['travel'] = compile_travel(roads,graphs,recipes['travelRecipes'])
        report['source'] = raw['receipt']
        report['input_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
        report['implementation_sha256'] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (Path(__file__),Path(__file__).with_name('infrastructure_flat_plan.py'),
             Path(__file__).with_name('infrastructure_network.py'),
             Path(__file__).with_name('infrastructure_network_graph.py'),
             Path(__file__).with_name('infrastructure_vertex_pool.py'),
             Path(__file__).with_name('infrastructure_travel.py'),
             Path(__file__).with_name('infrastructure_network_recipes.json'))}
        if args.shots is not None:
            image = args.shots/f'{place}-flat-network.png'
            render(image,place,transport,roads,graphs,args.view)
            report['image'] = str(image)
        (args.output/f'{place}-flat-network.json').write_text(json.dumps(report,indent=2)+'\n')
        arrays = {}
        for i,graph in enumerate(graphs.values()):
            arrays.update({f'{i}_{k}':graph[k] for k in
                           ('vertices','edges','owner_offsets','owner_sources','owner_directions',
                            'travel_public','travel_restricted')})
            coordinates,owners = shapely.get_coordinates(graph['lines'],return_index=True)
            arrays[f'{i}_line_coordinates'] = coordinates
            arrays[f'{i}_line_offsets'] = np.r_[0,np.cumsum(np.bincount(owners,minlength=len(graph['lines'])))]
        arrays['source_widths'] = np.array([road['width'] for road in roads])
        np.savez_compressed(args.output/f'{place}-flat-network.npz',**arrays)
        print(json.dumps(dict(place=place,planned=report['planned_axes'],states=report['states'],
                              reasons=report['reasons'],repairs=report['repairs'],
                              planning_ms=report['planning_and_source_binding_ms'],
                              audit_ms=report['source_coverage_audit_ms'])))


if __name__=='__main__':
    main()
