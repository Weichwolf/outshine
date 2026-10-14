"""Three actual shell sections of the source-house recovery; kept outside the four-image page."""

import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from boolean_building_bands import construct
from building_vertical import build,level
from structural_shell_geometry import mesh_section
from verify_room_fallback import cases
from evidence import revision


def render(output):
    output.mkdir(parents=True,exist_ok=True);sample=next(c for c in cases() if c['id'].endswith('/1267876600/4/3'))
    plan=build(sample);shell=construct(plan);origin=np.asarray(plan['footprint'].centroid.coords[0])
    fig,axes=plt.subplots(1,plan['storeys'],figsize=(24,13.5),squeeze=False)
    for index,axis in enumerate(axes[0]):
        current=level(plan,index);z=current['floor_z_m']+1.5;walls=mesh_section(shell.mesh,shell.vertex_roots,z)
        for polygon in shapely.get_parts(walls):
            xy=np.asarray(polygon.exterior.coords)-origin;axis.fill(*xy.T,color='#607c74')
            for hole in polygon.interiors:axis.fill(*(np.asarray(hole.coords)-origin).T,color='white')
        for number,room in enumerate(current['rooms']):
            point=np.asarray(room.representative_point().coords[0])-origin;axis.text(*point,str(number+1),ha='center',fontsize=17)
        path=plan['stair']['path'][:,:2]-origin;axis.plot(*path.T,color='#b2354d',linewidth=3)
        axis.set(aspect='equal',title=f'Geschoss {index+1} · Schnitt {z:.2f} m',xlabel='Ost ab Grundrisszentrum [m]',ylabel='Nord [m]');axis.grid(alpha=.15)
    fig.suptitle('Hameln · drei verbundene Geschosse im unveränderten 8-m-Quellkörper',fontsize=24)
    fig.text(.04,.06,'Tatsächliche Schnittkonturen des geschlossenen Meshs. Vier Flächen je Geschoss: Flur, Treppenkern und zwei generische Privaträume.\n'
        'Der einfachere Grundriss nutzt freie Versuche innerhalb derselben zwölf Routingalternativen. Kein vollständiges Architektur-/Nutzungsprogramm.',fontsize=16)
    fig.tight_layout(rect=(0.,.14,1.,.93));image=output/'recovered-house-sections.png';fig.savefig(image,dpi=120);plt.close(fig)
    (output/'recovered-house-sections.json').write_text(json.dumps(revision()|dict(id=plan['id'],source_sha256=sample['source_sha256'],
        storeys=plan['storeys'],room_ids=plan['storey_room_ids'],source_height_m=plan['source_top_m'],origin_enu_xy=origin.tolist(),
        method=plan['circulation']['seed_method'],attempts=len(plan['circulation_alternatives']),profile=[2880,1620]),indent=2)+'\n')
    print(image)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/room-fallback'))
    render(parser.parse_args().output)
