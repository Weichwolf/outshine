"""Actual upper surfaces and solid height slices, outside the four-image public page."""

import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from evidence import revision
from provider_contract import attributes
from structural_shell_geometry import mesh_section
from tile_buildings import read_tile
from tile_roof_envelopes import choose,roof_surface
from outshine_ref.buildings.modelling import heightfield_envelope


def render(output):
    output.mkdir(parents=True,exist_ok=True);rows,_=read_tile('Hameln');examples=[]
    for row in rows:
        if row['layer']!='building':continue
        try:parameters=attributes(row['properties']);choice=choose(row,parameters)
        except ValueError:continue
        if choice.kind=='flat' or choice.kind in [c.kind for r,p,c,e in examples]:continue
        try:top,_,_=roof_surface(row,choice);envelope=heightfield_envelope(top,parameters['minHeight'])
        except ValueError:continue
        examples.append((row,parameters,choice,envelope))
        if len(examples)==2:break
    fig,axes=plt.subplots(2,3,figsize=(24,13.5));metadata=[]
    for index,(row,parameters,choice,envelope) in enumerate(examples):
        mesh=envelope.mesh;origin=np.asarray(row['polygon'].centroid.coords[0]);top=mesh['faces'][envelope.surface_roles==0]
        points=mesh['vertices'];axis=axes[index,0]
        drawing=axis.tripcolor(points[:,0]-origin[0],points[:,1]-origin[1],top,points[:,2],shading='gouraud',cmap='viridis',
            vmin=choice.eave_m,vmax=parameters['buildingHeight'])
        fig.colorbar(drawing,ax=axis,shrink=.7,label='Dachhöhe [m]');axis.set_title('Pultdach' if choice.kind=='shed' else 'Satteldach')
        for column,height in enumerate((choice.eave_m-.05,choice.eave_m+choice.rise_m*.5),1):
            section=mesh_section(mesh,envelope.vertex_roots,height);axis=axes[index,column]
            for polygon in shapely.get_parts(section):
                axis.fill(*(np.asarray(polygon.exterior.coords)-origin).T,color='#607c74')
                for hole in polygon.interiors:axis.fill(*(np.asarray(hole.coords)-origin).T,color='white')
            axis.set_title(f'Tatsächlicher Körperquerschnitt · {height:.2f} m')
        for axis in axes[index]:axis.set(aspect='equal',xlabel='Ost ab Grundrisszentrum [m]',ylabel='Nord [m]');axis.grid(alpha=.15)
        metadata.append(dict(id=row['id'],kind=choice.kind,rise_m=choice.rise_m,eave_m=choice.eave_m,source_top_m=parameters['buildingHeight'],
            triangles=len(mesh['faces']),volume_m3=envelope.volume_m3))
    fig.suptitle('Hameln · Dachflächen und geschlossene Gebäudehüllen innerhalb der Quellhöhe',fontsize=23)
    fig.text(.04,.04,'Eigene allgemeine Hülle aus den tatsächlich triangulierten Dachflächen und der unteren Ebene. Keine internen doppelten Dach-/Körperkappen.\n'
        'Prozedurale kleine Außenkörper; Nutzung, Türen und Innenräume dieser Beispiele bleiben ungeplant. Die größeren begehbaren Nahkontrollen bleiben erhalten.',fontsize=15)
    fig.tight_layout(rect=(0.,.13,1.,.93));path=output/'roof-envelope-sections.png';fig.savefig(path,dpi=120);plt.close(fig)
    (output/'roof-envelope-sections.json').write_text(json.dumps(revision()|dict(cases=metadata,profile=[2880,1620]),indent=2)+'\n');print(path)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/roof-envelopes'));render(parser.parse_args().output)
