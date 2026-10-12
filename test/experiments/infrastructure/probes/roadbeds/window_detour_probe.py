"""Rank omitted direct motor routes by their proved-window alternatives."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
import infrastructure_coupled_roadbed_experiment as coupled
from infrastructure_coupled_roadbed_products import apply_coupled_product
from infrastructure_route_detours import route_detours,verify
from infrastructure_shared_frames import frame_key


def render_detour(network,item,output):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.collections import LineCollection
    source=shapely.LineString(item['source_path']);paths=[];routes={}
    for step in item['steps']:
        owner=step['module']
        if owner not in routes:
            poses,_=network.poses(owner,{})
            routes[owner]=coupled.directed_module_routes(network.graph,poses,network.shape(owner),network.tree)[0]
        paths.append(routes[owner][step['entry'],step['exit']])
    fig,axes=plt.subplots(1,2,figsize=(13,6))
    for ax,area,title in zip(axes,(shapely.union_all(paths).envelope.buffer(25),source.envelope.buffer(20)),
                            ('Window alternative','Missing direct connection')):
        ids=network.tree.query(area,predicate='intersects')
        ax.add_collection(LineCollection([shapely.get_coordinates(g) for g in network.graph['lines'][ids]],
            colors='#c7c9c5',linewidths=1))
        ax.add_collection(LineCollection([shapely.get_coordinates(g) for g in paths],colors='#167184',linewidths=2))
        ax.plot(*source.xy,color='#bc4242',linewidth=2,label='Missing direct source route')
        x,y=source.coords[0];ax.plot(x,y,'o',color='#bc4242')
        x0,y0,x1,y1=area.bounds;ax.set_xlim(x0,x1);ax.set_ylim(y0,y1);ax.set_aspect('equal');ax.set_title(title)
        ax.set_xlabel('east [m]');ax.set_ylabel('north [m]')
    fig.suptitle(f"{network.name} · {item['module']}: {item['entry']} → {item['exit']}\n"
        f"Source {item['source_length_m']:.1f} m; alternative ≈ {item['alternative_length_m']:.1f} m\n"
        'Source axes shown; alternatives use proved connections inside the frozen window')
    fig.tight_layout();output.parent.mkdir(parents=True,exist_ok=True);fig.savefig(output,dpi=140);plt.close(fig)


def window(root,row,images=None):
    name=row['place'];before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
    frames=json.loads((root/'boundary-optimized/hermite-routes.json').read_text())
    network=coupled.prepared_network(root,before,next(p['frames'] for p in frames['places'] if p['place']==name))
    cuts=json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text())
    coupled.restore_cut_plan(network,cuts['plan'])
    records={r['module']:r for r in before['modules']};records.update((r['module'],r) for r in cuts['changed_modules'])
    occupied=set();lengths={}
    for binding in row['coupled_products']:
        path=Path(binding['report']);assert coupled.digest(path)==binding['report_sha256']
        network,records,_,note=apply_coupled_product(network,records,set(),root,path,occupied)
        occupied.update(note['modules'])
        for p in json.loads(path.read_text())['proofs']:
            lengths[p['module'],tuple(map(tuple,p['request']))]=shapely.LineString(p['path']).length
    additions={(t['module'],t['entry'],t['exit']):shapely.LineString(t['path']).length
               for t in row['trials'] if t['accepted']}
    nodes={};edges={};missing=[];source_paths={}
    for record in before['modules']:
        owner=record['module'];record=records[owner];poses,_=network.poses(owner,{})
        ids=[nodes.setdefault(frame_key(p),len(nodes)) for p in poses]
        for turn in record['turns']:
            pair=turn['entry'],turn['exit'];a,b=(ids[i] for i in pair);key=owner,*pair
            if turn['accepted'] or key in additions:
                request=next(iter(coupled.request_keys(poses,[pair])))
                length=additions.get(key,lengths.get((owner,request),turn.get('length_m')))
                assert length is not None and length>0,key
                item=dict(module=owner,entry=pair[0],exit=pair[1],length_m=length)
                if (a,b) not in edges or length<edges[a,b]['length_m']:edges[a,b]=item
            else:
                if owner not in source_paths:
                    source_paths[owner]=coupled.directed_module_routes(network.graph,poses,network.shape(owner),network.tree)[0]
                route=source_paths[owner][pair]
                missing.append(dict(module=owner,entry=pair[0],exit=pair[1],nodes=[a,b],source_length_m=route.length,
                                    source_path=shapely.get_coordinates(route).tolist()))
    alternatives=route_detours(len(nodes),[(a,b,v['length_m']) for (a,b),v in edges.items()],[r['nodes'] for r in missing])
    for item in missing:
        alternative=alternatives[tuple(item['nodes'])];length=alternative['length_m']
        assert length is not None,'previous reachability claim contradicted'
        route=alternative['nodes']
        item.update(alternative_length_m=length,extra_length_m=length-item['source_length_m'],
                    ratio=length/item['source_length_m'],steps=[edges[a,b] for a,b in zip(route,route[1:])])
    missing.sort(key=lambda r:-r['extra_length_m'])
    if images is not None and missing:render_detour(network,missing[0],images/f'{name}-worst-detour.png')
    return dict(place=name,nodes=len(nodes),proved_edges=len(edges),missing=missing)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--images',type=Path)
    args=parser.parse_args();root=Path('build/experiments/infrastructure');start=time.perf_counter()
    result=dict(checks=verify(),places=[],reference=str(args.report),reference_sha256=coupled.digest(args.report),
        measured_commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        scope='Shortest alternatives inside the frozen windows; analytic baseline lengths and sampled proved-curve lengths. No global or exact arc-length claim.')
    for row in json.loads(args.report.read_text())['places']:
        place=window(root,row,args.images);result['places'].append(place)
        print(json.dumps(dict(place=place['place'],missing=len(place['missing']),worst=[
            {k:r[k] for k in ('module','entry','exit','source_length_m','alternative_length_m','ratio')} for r in place['missing'][:3]])),flush=True)
    result['elapsed_ms']=(time.perf_counter()-start)*1000
    result['implementation_sha256']={str(p.relative_to(Path.cwd())):coupled.digest(p) for p in coupled.implementation_files(Path(__file__))}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')


if __name__=='__main__':main()
