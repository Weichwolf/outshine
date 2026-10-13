"""Render frozen complete motor windows with their published replacement meshes."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.collections import LineCollection,PatchCollection,PolyCollection
from matplotlib.patches import Circle,Patch
import numpy as np
import shapely
from shapely.plotting import patch_from_polygon


def checked_file(path,expected):
    path=Path(path)
    assert hashlib.sha256(path.read_bytes()).hexdigest()==expected,f'changed rendering input: {path}'
    return path


def load_window(root,report):
    name=report['place']
    for relative,digest in report['input_sha256'].items():checked_file(root/relative,digest)
    before=json.loads((root/'window-1000-body'/f'{name}-routes.json').read_text())
    cuts=json.loads((root/'cut-setbacks'/f'{name}-cut-setbacks.json').read_text())
    records={r['module']:dict(r) for r in before['modules']}
    records.update((r['module'],dict(r)) for r in cuts['changed_modules'] if r['module'] in records)
    window_modules=set(records)
    shapes={r['module']:shapely.from_wkb(r['wkb']) for r in cuts['plan']['parts']}
    meshes=[];replaced=set();paths=[]
    for product in report['coupled_products']:
        proof=json.loads(checked_file(product['report'],product['report_sha256']).read_text())
        mesh=np.load(checked_file(proof['physical_mesh'],proof['physical_mesh_sha256']))
        meshes.append((mesh['vertices'][mesh['indices']],mesh['face_owners']))
        replaced.update(product['modules'])
        counts=Counter(p['module'] for p in proof['proofs'])
        omitted=Counter(p['module'] for p in proof.get('unresolved',[]))
        for owner in product['modules']:
            if owner not in records:records[owner]=dict(connections=counts[owner]+omitted[owner])
            assert records[owner]['connections']==counts[owner]+omitted[owner]
            records[owner]['accepted']=counts[owner]
        paths.extend(p['path'] for p in proof['proofs'])
    accepted=[t for t in report['trials'] if t['accepted']]
    for owner in sorted({t['module'] for t in accepted}):
        trials=[t for t in accepted if t['module']==owner]
        assert len({(t['mesh'],t['mesh_sha256']) for t in trials})==1
        mesh=np.load(checked_file(trials[0]['mesh'],trials[0]['mesh_sha256']))
        triangles=mesh['vertices'][mesh['indices']]
        meshes.append((triangles,np.full(len(triangles),owner)))
        replaced.add(owner);records[owner]['accepted']+=len(trials)
        paths.extend(t['path'] for t in trials)
    assert sum(records[m]['accepted'] for m in window_modules)==report['after']
    assert sum(records[m]['connections'] for m in window_modules)==report['connections']
    baseline=np.load(root/'directed-surfaces'/f'{name}-flat-surfaces.npz')
    return before,records,shapes,meshes,replaced,paths,baseline


def draw_window(ax,root,report):
    before,records,shapes,meshes,replaced,paths,baseline=load_window(root,report)
    def color(owner):
        row=records.get(int(owner))
        if row is None:return '#c8ccc7'
        if not row['connections']:return '#cca550'
        return '#76a389' if row['accepted']==row['connections'] else '#be6458'
    owners=baseline['face_owners'];keep=~np.isin(owners,list(set(shapes)|replaced))
    meshes.insert(0,(baseline['vertices'][baseline['indices'][keep]],owners[keep]))
    for triangles,owners in meshes:
        ax.add_collection(PolyCollection(triangles,facecolors=[color(o) for o in owners],edgecolors='none',rasterized=True))
    for owner,shape in shapes.items():
        if owner in replaced:continue
        patches=[patch_from_polygon(p) for p in shapely.get_parts(shape) if p.geom_type=='Polygon']
        ax.add_collection(PatchCollection(patches,facecolors=color(owner),edgecolors='none'))
    ax.add_collection(LineCollection(paths,colors='#216f96',linewidths=.45,alpha=.8,rasterized=True))
    radius=before['radius_m']
    ax.add_patch(Circle((0,0),radius,fill=False,color='#8a9692',linewidth=.5,linestyle=':'))
    ax.set(xlim=(-radius,radius),ylim=(-radius,radius),aspect='equal',facecolor='#f2f0e8',
           xlabel='East [m]',ylabel='North [m]',
           title=f"{report['place']} · layer 0 · radius {radius:g} m\n{report['after']}/{report['connections']} {report.get('motor_vehicle_class','motor')} routes")


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path('build/experiments/transport'))
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();data=json.loads(args.report.read_text())
    figure,axes=plt.subplots(1,len(data['places']),figsize=(7*len(data['places']),7),squeeze=False)
    for axis,report in zip(axes[0],data['places']):draw_window(axis,args.root,report)
    figure.legend(handles=[Patch(color=color,label=label) for color,label in
        (('#76a389','All requested motor routes proved'),('#be6458','Unresolved motor routes'),
         ('#c8ccc7','Other/unassessed transport'),('#216f96','Published constructed routes'))],loc='lower center',ncol=2,fontsize=8)
    figure.suptitle(f"Published Python roadbeds · measured commit {data['measured_commit'][:9]}\nNo runtime integration")
    figure.tight_layout(rect=(0,.09,1,.95));args.output.parent.mkdir(parents=True,exist_ok=True)
    figure.savefig(args.output,dpi=180);plt.close(figure)
    print(json.dumps(dict(image=str(args.output),report_sha256=hashlib.sha256(args.report.read_bytes()).hexdigest())))


if __name__=='__main__':
    main()
