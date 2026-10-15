"""Actual roof/room mesh slices and shared opening locations; private optical evidence."""

import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.path import Path as DrawPath
from matplotlib.patches import PathPatch
import numpy as np
import shapely

from evidence import revision
from pitched_room_details import complete
from provider_contract import attributes
from structural_shell_geometry import mesh_section
from tile_buildings import read_tile
from tile_roof_envelopes import choose,roof_surface
from verify_pitched_rooms import selected


def fill(axis,geometry,origin,colour):
    vertices=[];codes=[]
    for polygon in shapely.get_parts(shapely.orient_polygons(geometry)):
        if polygon.geom_type!='Polygon':continue
        for ring in (polygon.exterior,*polygon.interiors):
            points=np.asarray(ring.coords)-origin;vertices.extend(points)
            codes.extend([DrawPath.MOVETO,*([DrawPath.LINETO]*(len(points)-2)),DrawPath.CLOSEPOLY])
    if vertices:axis.add_patch(PathPatch(DrawPath(np.asarray(vertices),codes),facecolor=colour,edgecolor='none'))


def render(output):
    output.mkdir(parents=True,exist_ok=True);examples=[]
    for site in ('Hameln','Spittelau','Grohnde'):
        rows,_=read_tile(site)
        for row in selected(site,rows):
            p=attributes(row['properties']);choice=choose(row,p)
            if choice.kind in [e[2].kind for e in examples]:continue
            try:top,_,_=roof_surface(row,choice);mesh,info,plan,shell=complete(row,p,3,choice,top)
            except ValueError:continue
            examples.append((row,p,choice,plan,shell))
            if len(examples)==2:break
        if len(examples)==2:break
    if len(examples)!=2:raise ValueError('Both actual roof kinds require an optical room control')
    fig,axes=plt.subplots(2,3,figsize=(24,13.5));metadata=[]
    for index,(row,p,choice,plan,shell) in enumerate(examples):
        origin=np.asarray(row['polygon'].centroid.coords[0]);floor=plan['floor_z_m']
        window_head=max(window['head_m'] for window in plan['windows'])
        door_head=min(door['height_m'] for door in plan['doors'])
        middle=floor+(window_head+door_head)/2
        if not window_head<door_head:raise ValueError('Optical control requires a cut above windows and below doors')
        for column,height in enumerate((floor+1.3,middle,choice.eave_m+choice.rise_m*.5)):
            axis=axes[index,column];fill(axis,mesh_section(shell.mesh,shell.vertex_roots,height),origin,'#686f76')
            axis.plot(*(np.asarray(row['polygon'].exterior.coords)-origin).T,color='#356756',lw=1.)
            if column<2:
                for room,polygon in enumerate(plan['rooms']):
                    point=np.asarray(polygon.representative_point().coords[0])-origin;axis.text(*point,f'Raum {room+1}',ha='center',fontsize=10)
                for openings,colour in ((plan['doors'],'#dca73d'),(plan['windows'],'#48a1c5')):
                    for opening in openings:
                        point=np.asarray(opening['centre'])-origin;axis.plot(*point,'o',color=colour,ms=5)
            axis.set_title(f"{'Pult' if choice.kind=='shed' else 'Sattel'} · tatsächlicher Schnitt bei {height:.2f} m")
            axis.set(aspect='equal',xlabel='Ost ab Grundrisszentrum [m]',ylabel='Nord [m]');axis.grid(alpha=.15)
        metadata.append(dict(id=row['id'],kind=choice.kind,rooms=len(plan['rooms']),doors=len(plan['doors']),windows=len(plan['windows']),
            source_top_m=p['buildingHeight'],eave_m=choice.eave_m,triangles=len(shell.mesh['faces'])))
    fig.suptitle('Quellhäuser · echte Räume und Öffnungen unter derselben geschlossenen Dachform',fontsize=23)
    fig.text(.04,.045,'Grau: tatsächliche Struktur. Gold: Türen. Blau: Fenster. Links: Schnitt durch Fenster; Mitte: oberhalb der Fenster, durch Türöffnungen.\n'
        'Rechts: vollständiger Dachkörper. Räume und Erschließung sind generische prozedurale Ergänzungen; Dachraum noch ohne Nutzung/Zugang.',fontsize=15)
    fig.tight_layout(rect=(0.,.13,1.,.93));path=output/'pitched-room-sections.png';fig.savefig(path,dpi=120);plt.close(fig)
    (output/'pitched-room-sections.json').write_text(json.dumps(revision()|dict(cases=metadata,profile=[2880,1620]),indent=2)+'\n');print(path)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/pitched-rooms'))
    render(parser.parse_args().output)
