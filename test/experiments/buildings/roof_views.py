"""Metric roof plans and actual mesh sections before spatial and lit exterior views."""

import textwrap

import matplotlib.pyplot as plt
from matplotlib.collections import LineCollection
import numpy as np

from building_layout import frame
from building_mesh import slice_triangles
from building_plan_views import patch


NAMES = {'flat':'Flach','hip':'Walm','gable':'Sattel · ein First','shed':'Pult'}


def roofplan(axis, plan):
    for index,facet in enumerate(plan['roof']['facets']):
        patch(axis,facet['polygon'],facecolor=plt.get_cmap('Pastel2')(index%8),edgecolor='#4e5960',linewidth=1.)
        point = facet['polygon'].representative_point()
        axis.text(point.x,point.y,f'F{index+1}',ha='center',va='center',fontsize=8)
    patch(axis,plan['footprint'],facecolor='none',edgecolor='#222b32',linewidth=1.5)
    axis.autoscale_view()
    axis.set_aspect('equal')
    axis.set_xlabel('Ost [m]')
    axis.set_ylabel('Nord [m]')
    axis.grid(alpha=.15)
    axis.set_title('2D · Dachflächen und unveränderter Quellgrundriss',fontsize=10)


def section(axis, plan, product):
    forward,_ = frame(plan['footprint'])
    normal,tangent = np.array(forward[2:4]),np.array(forward[:2])
    centre = np.array(plan['footprint'].centroid.coords[0])
    pieces = slice_triangles(product,[*normal,0.],centre@normal)
    for material in (0,1,2,3,4,5,6,7,9):
        lines = [np.c_[(points[:,:2]-centre)@tangent,points[:,2]] for points,kind in pieces if kind==material]
        axis.add_collection(LineCollection(lines,colors='#8d5c37' if material==9 else '#75808a',linewidth=1.1))
    axis.autoscale_view()
    axis.set_ylim(plan['body_top_m']-.7,plan['source_top_m']+.25)
    axis.set_xlabel('Entlang der Gebäudeachse [m]')
    axis.set_ylabel('Höhe [m]')
    axis.grid(alpha=.15)
    axis.set_title('2D · tatsächlicher Mesh-Schnitt im Dachanschluss',fontsize=10)


def figure(sample, plan, product, output, spatial, exterior):
    fig = plt.figure(figsize=(16,10),dpi=100,facecolor='white')
    grid = fig.add_gridspec(2,2,left=.055,right=.96,top=.88,bottom=.16,hspace=.38,wspace=.22)
    roofplan(fig.add_subplot(grid[0,0]),plan)
    section(fig.add_subplot(grid[0,1]),plan,product)
    for cell,path,title in ((grid[1,0],spatial,'2.5D · vollständiges konstruktives Mesh'),
                            (grid[1,1],exterior,'Eevee · äußere Nahansicht; generische Fassaden')):
        axis = fig.add_subplot(cell)
        axis.imshow(plt.imread(path))
        axis.axis('off')
        axis.set_title(title,fontsize=10)
    roof = plan['roof']
    fig.suptitle(sample['label']+' · '+NAMES[roof['kind']],fontsize=17,y=.97)
    fig.text(.055,.925,f"{len(roof['facets'])} Dachflächen · {roof['method']} · {plan['storeys']} generische Geschosse · Dachhöhe {roof['rise_m']:.2f} m",fontsize=12)
    fig.text(.055,.105,f"Gelieferte Gesamthülle {plan['base_z_m']:.2f}–{plan['source_top_m']:.2f} m; Körperoberkante {plan['body_top_m']:.2f} m. Dach vor Innenraumplanung reserviert.",fontsize=10)
    fig.text(.055,.078,'Dachformen sind gekennzeichnete Ergänzungen. Geschosszahl bei fehlenden Angaben neu geschätzt; vollständige Architekturrezepte und native LOD bleiben offen.',fontsize=9)
    fig.text(.055,.045,'\n'.join(textwrap.wrap('; '.join(plan['omissions']),185)),fontsize=7,color='#745744')
    fig.savefig(output,dpi=100)
    plt.close(fig)
