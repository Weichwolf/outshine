"""Actual mesh sections for direct rooms and the previously obstructed door approach."""

import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Circle
import numpy as np
import shapely

from boolean_building_bands import construct
from building_vertical import build
from evidence import revision
from structural_shell_geometry import mesh_section
from verify_room_fallback import baseline_module
from verify_single_storey import cases,previous_plan


def section(axis,plan,origin):
    shell=construct(plan);z=plan['floor_z_m']+1.5
    walls=mesh_section(shell.mesh,shell.vertex_roots,z)
    for polygon in shapely.get_parts(walls):
        xy=np.asarray(polygon.exterior.coords)-origin;axis.fill(*xy.T,color='#607c74')
        for hole in polygon.interiors:axis.fill(*(np.asarray(hole.coords)-origin).T,color='white')
    axis.set(aspect='equal',xlabel='Ost ab Ansichtsmitte [m]',ylabel='Nord [m]');axis.grid(alpha=.15)
    return z


def doorway(axis,plan,door,title):
    centre=np.asarray(door['centre']);tangent=np.asarray(door['tangent']);normal=np.array([tangent[1],-tangent[0]])
    z=section(axis,plan,centre);endpoints=np.array([-.45,.45])[:,None]*normal
    axis.plot(*endpoints.T,color='#b2354d',linewidth=3)
    for point in endpoints:axis.add_patch(Circle(point,.30,fill=False,color='#b2354d',linewidth=2))
    axis.set(xlim=(-2.,2.),ylim=(-2.,2.),title=title)
    return dict(door=door['id'],centre_enu_xy=centre.tolist(),tangent=tangent.tolist(),section_z_m=z,
        approach_m=.45,actor_radius_m=.30)


def render(output,commit):
    output.mkdir(parents=True,exist_ok=True);samples=list(cases());direct=samples[0]
    affected=next(s for s in samples if s['id'].endswith('/17363470/3/1506'))
    old_vertical,_=baseline_module(commit,'building_vertical');old_layout,_=baseline_module(commit,'building_layout')
    before=previous_plan(affected,old_vertical,old_layout);after=build(affected);plan=build(direct)
    old_door=next(d for d in before['doors'] if d['id']=='door-0-3')
    new_door=min((d for d in after['doors'] if d['kind']=='corridor-door'),
        key=lambda d:np.linalg.norm(np.asarray(d['centre'])-old_door['centre']))
    fig,axes=plt.subplots(1,3,figsize=(24,13.5));origin=np.asarray(plan['footprint'].centroid.coords[0])
    z=section(axes[0],plan,origin)
    for index,room in enumerate(plan['rooms']):
        point=np.asarray(room.representative_point().coords[0])-origin
        axes[0].text(*point,str(index+1),ha='center',fontsize=17)
    axes[0].set_title(f'Fünf direkt verbundene Räume · Schnitt {z:.2f} m')
    old=doorway(axes[1],before,old_door,'Vorher: Körper am Türantritt in der Außenwand')
    new=doorway(axes[2],after,new_door,'Jetzt: geprüfter Durchgang mit vollem Körperradius')
    fig.suptitle('Hameln · einfachere einstöckige Erschließung und vollständige Türantritte',fontsize=23)
    fig.text(.04,.06,'Grün: tatsächliche Schnitte des geschlossenen Strukturmeshs. Rot: 0,30-m-Körperradius an beiden Enden des 0,90-m-Antrittwegs.\n'
        'Links ohne zusätzlichen Kern. Mitte/rechts unterschiedliche zulässige Planalternativen; reale Kollisionsprüfung separat im Prüfbericht. Generische Räume.',fontsize=15)
    fig.tight_layout(rect=(0.,.14,1.,.93));image=output/'single-storey-sections.png';fig.savefig(image,dpi=120);plt.close(fig)
    (output/'single-storey-sections.json').write_text(json.dumps(revision()|dict(direct_id=direct['id'],affected_id=affected['id'],
        source_sha256=direct['source_sha256'],baseline_commit=commit,old_door=old,new_door=new,profile=[2880,1620]),indent=2)+'\n')
    print(image)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/single-storey'))
    parser.add_argument('--baseline',default='64c5d840ca57cd98d4ba47345ce9d4b911fcb7e7');args=parser.parse_args();render(args.output,args.baseline)
