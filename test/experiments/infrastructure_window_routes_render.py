"""Render complete radius coverage and selected unresolved motor-route recipes."""

import argparse
import hashlib
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.collections import LineCollection,PolyCollection
from matplotlib.patches import Circle,Patch
import numpy as np
import shapely

from infrastructure_driving_network import DrivingNetwork
from infrastructure_junction_surfaces import motor_surface
from infrastructure_junction_turn_experiment import render
from infrastructure_route_connectivity import network_connectivity,verify_connectivity
from infrastructure_shared_frames import frame_key


def prepared_network(root,report,reference,vehicle):
    for name,digest in report['input_sha256'].items():
        assert hashlib.sha256((root/name).read_bytes()).hexdigest()==digest
    network = DrivingNetwork(root,report['place'],vehicle)
    network.frames = {frame_key(f):{k:np.array(f[k]) for k in ('position','heading')} for f in reference['frames']}
    return network


def draw_window(ax,network,report):
    radius = report['radius_m']
    faces = network.mesh[1].query(shapely.Point(0,0),predicate='dwithin',distance=radius)
    colors = np.full(len(network.product['part_roles']),'#c8ccc7',dtype=object)
    for row in report['modules']:
        colors[row['module']] = ('#cca550' if not row['connections'] else
                                '#76a389' if row['accepted']==row['connections'] else '#be6458')
    triangles = network.product['vertices'][network.product['indices'][faces]]
    ax.add_collection(PolyCollection(triangles,facecolors=colors[network.product['face_owners'][faces]],
                                     edgecolors='none',rasterized=True))
    paths = [np.array(p) for row in report['paths'] for p in row['paths'] if p is not None]
    ax.add_collection(LineCollection(paths,colors='#216f96',linewidths=.3,alpha=.65,rasterized=True))
    ax.add_patch(Circle((0,0),radius,fill=False,color='#8a9692',linewidth=.6,linestyle=':'))
    counts = report['after']
    ax.set(xlim=(-radius*1.03,radius*1.03),ylim=(-radius*1.03,radius*1.03),aspect='equal',
           facecolor='#f2f0e8',xlabel='East [m]',ylabel='North [m]',
           title=f"{report['place']} — {radius:g} m radius\n{counts['accepted']}/{counts['connections']} checked routes; {counts['modules']} motor modules")


def deficit_views(network,report,reference,connectivity):
    old = {r['module'] for r in reference['modules']}
    critical = {r['module'] for edge in connectivity['missing_edges'] if edge['reason']=='disconnected'
                for r in edge['routes']}
    candidates = sorted((r for r in report['modules'] if r['accepted']<r['connections']),
                         key=lambda r:(r['module'] not in critical,r['module'] in old,
                                       -(r['connections']-r['accepted']),r['module']))
    stored = {r['module']:r['paths'] for r in report['paths']}
    views,reasons = [],set()
    for row in candidates:
        reason = max(row['reasons'],key=row['reasons'].get)
        if reason in reasons:
            continue
        reasons.add(reason)
        module = row['module']
        shape = network.shape(module)
        poses,_ = network.poses(module,{})
        floor = motor_surface(shape,network.graph,network.tree,network.mesh,network.vehicle,
                              network.curbs['corner_radius_m'],network.curbs['arc_error_m'])
        paths = [None if p is None else shapely.LineString(p) for p in stored[module]]
        views.append((report['place'],module,row,(shape,floor,poses,paths,row['turns'])))
        if len(views)==2:
            break
    return views


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,default=Path('build/experiments/infrastructure'))
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--reference',type=Path,default=Path('build/experiments/infrastructure/boundary-optimized/hermite-routes.json'))
    parser.add_argument('--output',type=Path,default=Path('build/shots/experiments/infrastructure'))
    args = parser.parse_args()
    data,reference = (json.loads(p.read_text()) for p in (args.report,args.reference))
    assert data['reference_sha256']==hashlib.sha256(args.reference.read_bytes()).hexdigest()
    for name,digest in data['implementation_sha256'].items():
        assert hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()==digest
    vehicle = json.loads(Path(__file__).with_name('infrastructure_network_recipes.json').read_text())['vehicleProfiles']['compact']
    fig,axes = plt.subplots(1,len(data['places']),figsize=(7*len(data['places']),7),squeeze=False)
    views,connectivity = [],[]
    for ax,report in zip(axes[0],data['places']):
        source = next(p for p in reference['places'] if p['place']==report['place'])
        network = prepared_network(args.input,report,source,vehicle)
        connectivity.append(network_connectivity(network,report))
        draw_window(ax,network,report)
        views.extend(deficit_views(network,report,source,connectivity[-1]))
    legend = [Patch(facecolor=color,label=label) for color,label in
              [('#76a389','All requested car routes proved'),('#be6458','Unresolved car route'),
               ('#cca550','No directed transit route'),('#c8ccc7','No requested car transit')]]
    fig.legend(handles=legend,loc='lower center',ncol=4)
    fig.suptitle('Complete physical-module coverage in the specified radius; blue = full vehicle sweep proved. Foot and rail routing remain open.')
    fig.tight_layout(rect=(0,.06,1,.95))
    args.output.mkdir(parents=True,exist_ok=True)
    (args.report.parent/'route-connectivity.json').write_text(json.dumps(dict(checks=verify_connectivity(),
         report_sha256=hashlib.sha256(args.report.read_bytes()).hexdigest(),places=connectivity,
         implementation_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in
             (Path(__file__),Path(__file__).with_name('infrastructure_route_connectivity.py'))},
         scope='Component preservation of requested ground motor routes; cross-directed-component reachability remains unproved.'),indent=2)+'\n')
    fig.savefig(args.output/'window-driving-network.png',dpi=140)
    plt.close(fig)
    if views:
        render(args.output/'window-driving-deficits.png',views,
               'Priority deficits: disconnected routes first; blue = proved; red dotted = unresolved request')


if __name__=='__main__':
    main()
