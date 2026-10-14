"""Current complete building shell, actual framed openings, stairs and night interiors."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from boolean_building_bands import construct
from boolean_building_plan import sources
from building_vertical import build,level
from demand import View
from evidence import revision
from interior_light_plan import lights
from showcase_groups import save_plot
from structural_shell_geometry import assembly,mesh_section
from verify_structural_shell import cases,geometry,access


def camera(points,plan,entry=False):
    door=plan['entrance'];centre=np.r_[door['centre'],plan['floor_z_m']+1.55]
    tangent=np.r_[door['tangent'],0.];normal=np.array([tangent[1],-tangent[0],0.])
    if entry:return View(centre+normal*3.,centre-normal*5.,width=1920,height=1080,fov_deg=55.)
    target=(points.min(axis=0)+points.max(axis=0))/2;direction=normal*1.5+tangent*.8+np.array([0.,0.,.65]);direction/=np.linalg.norm(direction)
    view=View(target+direction,target,width=1920,height=1080,fov_deg=50.);projected=(points-target)@view.basis.T
    required=np.maximum(abs(projected[:,0])*view.focal_px/(1920*.44),abs(projected[:,1])*view.focal_px/(1080*.42))-projected[:,2]
    return View(target+direction*(required.max()+1.),target,width=1920,height=1080,fov_deg=50.)


def physical(output,stem,title,page,mesh,plan,night,entry,provenance,proof):
    origin=np.r_[np.asarray(plan['footprint'].centroid.coords[0]),0.];local=dict(mesh,vertices=mesh['vertices']-origin)
    path=output/(stem+'-mesh.npz');np.savez_compressed(path,**local);view=camera(mesh['vertices'],plan,entry)
    lighting=lights(plan)
    for lamp in lighting:lamp['position']=(np.asarray(lamp['position'])-origin).tolist()
    image=output/(stem+'.png');config=dict(profile=[1920,1080],mesh=str(path.resolve()),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        eye=(view.eye-origin).tolist(),target=(view.target-origin).tolist(),fov_deg=view.fov_deg,
        wall_linear=[.7,.69,.65],roof_linear=[.24,.25,.24],night=night,spp=64,seed=17,denoise=True,glass='dielectric',
        procedural_panels=dict(resolution=512,size_m=[2.4,1.2],joint_m=.006,depth_m=.0015),
        material_overrides={'2':{'roughness':.8,'metallic':0.},'10':{'roughness':.4,'metallic':.65}},
        room_lights=lighting if night else [],output=str(image.resolve()))
    scene=output/(stem+'-scene.json');scene.write_text(json.dumps(config,indent=2)+'\n')
    log=Path(tempfile.gettempdir())/('outshine-buildings-'+stem+'-'+provenance['commit'][:9]+'.log')
    with log.open('w') as stream:subprocess.run([sys.executable,str(Path(__file__).with_name('mitsuba_interior.py')),str(scene)],stdout=stream,stderr=subprocess.STDOUT,timeout=900,check=True)
    renderer=json.loads(image.with_suffix('.renderer.json').read_text())
    if renderer['included_source_triangles']!=len(mesh['faces']):raise ValueError('Structural rendering omitted source triangles')
    record=provenance|dict(representation='showcase-current',page=page,view='interior' if entry else 'exterior',label='planned-shell',
        title=title,caption='Echter Wiener Quellgrundriss und gelieferte 8 m Gesamthöhe: drei Geschosse, fünf Räume je Geschoss, '
        'Flur, konstruierte Treppen, echte Wand-/Deckenöffnungen und vorbereitete Fensterrahmen. '
        'Die geschlossene Strukturhülle entsteht aus Wand-/Deckenbändern; prozedurale PBR-Platten und geometrisches Glas. '
        'Das generische Raumprogramm zählt als Konstruktionskontrolle; vollständige Architektur-Rezeptfamilien und native Spielersteuerung bleiben offen.',
        profile=[1920,1080],renderer=renderer,triangles=len(mesh['faces']),source_id=plan['id'],source_properties=plan['source_properties'],
        source_sha256=plan.get('source_sha256'),scene_origin_enu=origin.tolist(),proof=proof,room_lights=lighting if night else [],
        image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),completed_recipe_families=0,lod_status='native LOD unproved')
    name=stem+'-report.json';(output/name).write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');return name


def plans(output,plan,shell,provenance):
    started=time.perf_counter();fig,axes=plt.subplots(1,3,figsize=(32,18))
    for index,axis in enumerate(axes):
        current=level(plan,index);wall=mesh_section(shell.mesh,shell.vertex_roots,current['floor_z_m']+1.5)
        parts=list(wall.geoms) if hasattr(wall,'geoms') else [wall]
        for polygon in parts:
            xy=np.asarray(polygon.exterior.coords);axis.fill(*xy.T,color='#788f87',edgecolor='#344454')
            for hole in polygon.interiors:axis.fill(*np.asarray(hole.coords).T,color='white',edgecolor='#344454')
        for number,room in enumerate(current['rooms']):
            point=room.representative_point();axis.text(point.x,point.y,str(number+1),ha='center',fontsize=17)
        path=plan['stair']['path'];axis.plot(path[:,0],path[:,1],color='#aa2644',linewidth=3.)
        axis.set(aspect='equal',title=f'Geschoss {index+1} · Mesh-Schnitt {current["floor_z_m"]+1.5:.2f} m',xlabel='Ost [m]',ylabel='Nord [m]');axis.grid(alpha=.15)
    fig.suptitle('Wien · geschlossene Strukturhülle mit tatsächlichen Raum- und Treppenöffnungen',fontsize=26)
    fig.text(.035,.08,'Schnittkonturen stammen aus Double-Meshkanten mit expliziten Vertexidentitäten. Räume, Zugänge und Treppen stammen aus demselben Plan.\n'
        'Prüfkörper: 0,30 m Torso-Radius und 1,90 m Höhe. Native Strahlabfragen prüfen konstruierte Stufen und Fußkontakte; keine native Spielabnahme.',fontsize=18)
    fig.tight_layout(rect=(0.,.17,1.,.92))
    return save_plot(fig,output,'current-structural-plan','Wien · drei Geschosse aus derselben Hülle',
        'Tatsächliche horizontale Schnitte durch die erzeugte Hülle. Die eingezeichnete Treppe verbindet die aus demselben Plan erzeugten Ebenen.',
        provenance,started,[3200,1800],label='planned-shell')


def render(output):
    output.mkdir(parents=True,exist_ok=True);provenance=revision();sample=next(row for row in cases() if row['id']=='Vienna:800900:15')
    plan=build(sample);shell=construct(plan);proof=geometry(plan,sources(plan),shell);mesh,collision=assembly(plan,shell,frames=True);proof.update(access(plan,collision))
    reports=[plans(output,plan,shell,provenance)]
    for key,night,entry,title,page in [('day',False,False,'Wien · vollständige Strukturhülle bei Tag','buildings'),
        ('night',True,False,'Wien · Räume hinter den Fenstern bei Nacht','buildings'),
        ('entry',False,True,'Wien · Eingang, Flur und Treppenanschluss','interiors')]:
        reports.append(physical(output,'current-structural-'+key,title,page,mesh,plan,night,entry,provenance,proof))
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/structural-shell-release'))
    print(render(parser.parse_args().output),flush=True)
