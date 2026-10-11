"""Ground plans, paired mesh sections and isolated storey/core inspections."""

import textwrap

import matplotlib.pyplot as plt
from matplotlib.collections import LineCollection
import numpy as np

from building_mesh import slice_triangles
from building_plan_views import groundplan


COLOURS = ['#d1c5b0','#555b63','#b0b6c0','#3e99b6','#cbd2d6','#9d7040','#bea27b','#507665']


def stairs_on_plan(axis, plan, product):
    if 'stair' not in plan:
        return
    selected = (product['materials']==5)&(product['levels']==0)
    triangles = product['vertices'][product['faces'][selected]]
    normal = np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0])
    edges = [np.r_[triangle[:,:2],triangle[:1,:2]] for triangle in triangles[normal[:,2]>1e-8]]
    axis.add_collection(LineCollection(edges,colors='#9d7040',linewidths=.45))
    path = plan['stair']['path']
    axis.plot(path[:,0],path[:,1],color='#28714d',linewidth=1.,alpha=.8)


def core_section(axis, plan, product):
    stair = plan['stair']
    origin,along,across = (np.asarray(stair[key]) for key in ('origin','along','across'))
    depth = stair['size_m'][1]
    for flight in stair['flights']:
        centre = np.asarray(flight['origin'])
        pieces = slice_triangles(product,[*across,0.],centre@across)
        for points,material in pieces:
            x = (points[:,:2]-origin)@along
            if abs(x[1]-x[0])<1e-8:
                if not 0<=x[0]<=depth:
                    continue
                clipped = np.c_[x,points[:,2]]
            else:
                parameters = np.sort((np.array([0.,depth])-x[0])/(x[1]-x[0]))
                low,high = max(0.,parameters[0]),min(1.,parameters[1])
                if high<=low:
                    continue
                clipped = np.array([x[0],points[0,2]])+np.array([low,high])[:,None]*np.array([x[1]-x[0],points[1,2]-points[0,2]])
            axis.plot(clipped[:,0],clipped[:,1],color=COLOURS[material],linewidth=.9)
    axis.set_xlim(-.2,depth+.2)
    axis.set_ylim(plan['base_z_m']-.25,plan['source_top_m']+.25)
    axis.set_xlabel('Entlang des Kerns [m]')
    axis.set_ylabel('Quellhöhe [m]')
    axis.grid(alpha=.15)
    axis.set_title('2D · zwei parallele Mesh-Schnitte, überlagert',fontsize=10)


def figure(sample, plan, product, path, spatial_path=None, core_path=None):
    fig = plt.figure(figsize=(16,10),dpi=100,facecolor='white')
    grid = fig.add_gridspec(2,2,left=.055,right=.96,top=.88,bottom=.17,hspace=.39,wspace=.22)
    axis = fig.add_subplot(grid[0,0])
    groundplan(axis,plan,product)
    if product is not None:
        stairs_on_plan(axis,plan,product)
        axis = fig.add_subplot(grid[0,1])
        if 'stair' in plan:
            core_section(axis,plan,product)
        else:
            axis.axis('off')
            axis.text(.05,.7,'Ein Geschoss; keine Treppe erforderlich.\nTechnikreserve bleibt unausgestattet.',fontsize=13)
        for cell,image,title in [(grid[1,0],spatial_path,'2.5D · Erdgeschoss und erster Treppenlauf'),
                                 (grid[1,1],core_path,'2.5D · Treppenbauteile der ersten zwei Verbindungen')]:
            axis = fig.add_subplot(cell)
            axis.axis('off')
            if image is not None:
                axis.imshow(plt.imread(image))
                axis.set_title(title,fontsize=10)
        subtitle = f"{plan['storeys']} Geschosse · {plan['storeys']*len(plan['rooms'])} logische Flächen · {max(0,plan['storeys']-1)} konstruierte Geschossverbindungen"
    else:
        axis = fig.add_subplot(grid[:,1])
        axis.axis('off')
        axis.text(.03,.9,'Kein konstruktiver Geschossplan\n\n'+'\n'.join(textwrap.wrap('; '.join(plan['omissions']),55)),
                  va='top',fontsize=12,color='#a44439')
        subtitle = 'Quelle erhalten; ungeeignete Pläne mit Grund ausgewiesen'
    fig.suptitle(sample['label']+' · Geschosse und echte Treppen',fontsize=17,y=.97)
    fig.text(.055,.925,subtitle,fontsize=12)
    fig.text(.055,.105,f"Quellhülle {plan['base_z_m']:.2f}–{plan['source_top_m']:.2f} m. Geschosse wiederholen denselben generischen Raumplan; Nutzungsprogramme bleiben offen.",fontsize=10)
    fig.text(.055,.080,'Kernansicht: übrige Bauteile und Geschossböden ausgeblendet; vollständiges Mesh unverändert. Stichproben zur Kopffreiheit ersetzen keine native Spielerphysik.',fontsize=9)
    fig.text(.055,.047,'\n'.join(textwrap.wrap('; '.join(plan['omissions']),185)),fontsize=7,color='#745744')
    fig.savefig(path,dpi=100)
    plt.close(fig)
