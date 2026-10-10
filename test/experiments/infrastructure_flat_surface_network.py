#!/usr/bin/env python3
"""Render a flat shared surface from previously prepared vector graphs."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from infrastructure_flat_plan import examples,plan
from infrastructure_flat_surfaces import load_graphs,solve


def render(path,place,product,view):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection,LineCollection
    xy = product['vertices']
    triangles = xy[product['indices']]
    core = product['part_roles'][product['face_owners']].astype(bool)
    fig,axes = plt.subplots(1,2,figsize=(14,7),constrained_layout=True)
    for ax,title in zip(axes,('Junctions and corridors','One indexed mesh; shared ports')):
        ax.set(xlim=(-view,view),ylim=(-view,view),aspect='equal',title=title,
               xlabel='East [m]',ylabel='North [m]',facecolor='#f1f0e8')
        ax.grid(alpha=.15)
    axes[0].add_collection(PolyCollection(triangles,facecolors=np.where(core[:,None],
                            np.array([.25,.50,.42,1]),np.array([.57,.62,.61,1])),edgecolors='none'))
    axes[1].add_collection(PolyCollection(triangles,facecolors='#879594',edgecolors='#53645e',linewidths=.15))
    axes[1].add_collection(LineCollection(xy[product['port_edges']],colors='#bf6d28',linewidths=.8))
    fig.suptitle(f'{place}: flat physical surfaces; all internal interface vertices are shared')
    path.parent.mkdir(parents=True,exist_ok=True)
    fig.savefig(path,dpi=140)
    plt.close(fig)


def store(output,place,graphs,precision,view,source):
    product,report = solve(graphs,precision)
    image = output/'2d'/f'{place}-flat-surfaces.png'
    render(image,place,product,view)
    report.update(place=place,precision_m=precision,image=str(image),**source)
    report['implementation_sha256'] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
        (Path(__file__),Path(__file__).with_name('infrastructure_flat_surfaces.py'),
         Path(__file__).with_name('geos_triangulation.py'),
         Path(__file__).with_name('infrastructure_network_plan.py'))}
    (output/f'{place}-flat-surfaces.json').write_text(json.dumps(report,indent=2)+'\n')
    np.savez_compressed(output/f'{place}-flat-surfaces.npz',**product)
    print(json.dumps({k:v for k,v in report.items() if k!='implementation_sha256'}))


def small_cases(output,precision):
    recipes = json.loads(Path(__file__).with_name('infrastructure_network_recipes.json').read_text())
    for name,roads in examples().items():
        accepted,graphs,usage = plan(roads,recipes,{},precision)
        widths = np.array([r['width'] for r in accepted])
        for graph in graphs.values():
            graph['half_widths'] = np.maximum.reduceat(widths[graph['owner_sources']],
                                                       graph['owner_offsets'][:-1])*.5
        assert not usage['states'].get('not_used',0),(name,usage['reasons'])
        store(output,name,list(graphs.values()),precision,35,dict(source_fixture=name))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--small',action='store_true')
    parser.add_argument('--precision',type=float,default=.001)
    parser.add_argument('--view',type=float,default=600)
    parser.add_argument('places',nargs='*')
    args = parser.parse_args()
    if not args.small and (args.input is None or not args.places):
        parser.error('supply --small or --input and places')
    args.output.mkdir(parents=True,exist_ok=True)
    if args.small:
        small_cases(args.output,args.precision)
    for place in args.places:
        source_plan = args.input/f'{place}-flat-network.json'
        graph_path = args.input/f'{place}-flat-network.npz'
        previous = json.loads(source_plan.read_text())
        graphs = load_graphs(graph_path,previous)
        store(args.output,place,graphs,args.precision,args.view,
              dict(source_plan=str(source_plan),source_graph_sha256=hashlib.sha256(graph_path.read_bytes()).hexdigest()))


if __name__=='__main__':
    main()
