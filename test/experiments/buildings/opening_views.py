"""Actual module projections and sections next to spatial and lit building comparisons."""

import matplotlib.pyplot as plt
from matplotlib.collections import LineCollection
import numpy as np

from building_mesh import slice_triangles
from building_plan_views import patch
from opening_modules import template
from verify_openings import projected


def figure(label, openings, output, spatial, near, close):
    product=template()
    fig=plt.figure(figsize=(16,12),dpi=100,facecolor='white')
    grid=fig.add_gridspec(3,2,left=.055,right=.965,top=.895,bottom=.12,hspace=.38,wspace=.20)
    front=fig.add_subplot(grid[0,0])
    for material,colour in ((11,'#809497'),(10,'#414f58')):
        patch(front,projected(product,material),facecolor=colour,edgecolor='#34424c',linewidth=.8)
    front.autoscale_view();front.set_aspect('equal');front.grid(alpha=.15)
    front.set_title('2D · tatsächliche Fensterrahmen-/Bankprojektion',fontsize=10)
    front.set_xlabel('Breite [m]');front.set_ylabel('Höhe ab Fensterschwelle [m]')
    section=fig.add_subplot(grid[0,1])
    for material,colour in ((10,'#414f58'),(11,'#809497')):
        lines=[points[:,[1,2]] for points,kind in slice_triangles(product,[1,0,0],0.) if kind==material]
        section.add_collection(LineCollection(lines,colors=colour,linewidth=1.3))
    section.axvline(0.,color='#bac1c7',linestyle='--',label='bestehende Fassadenebene')
    section.autoscale_view();section.set_aspect('equal');section.grid(alpha=.15)
    section.set_xlabel('Wandtiefe; negativ außen [m]');section.set_ylabel('Höhe ab Fensterschwelle [m]')
    section.set_title('2D · echter Mittelquerschnitt; Bankgefälle 1:12',fontsize=10)
    for cell,path,title in ((grid[1,0],spatial,'2.5D · wiederverwendbares Fensterbauteil'),
                            (grid[1,1],close,'Eevee · Nahfenster am echten Raum'),
                            (grid[2,0],near,'Eevee · bestehender Quellkörper mit Öffnungsdetails')):
        axis=fig.add_subplot(cell);axis.imshow(plt.imread(path));axis.axis('off');axis.set_title(title,fontsize=10)
    notes=fig.add_subplot(grid[2,1]);notes.axis('off')
    windows=sum(row['kind']=='window' for row in openings)
    entrances=sum(row['kind']=='entrance' for row in openings)
    notes.text(0.,.92,f'{windows} Fenster · {entrances} Eingang\n\n'
               'Sash, äußere Einfassung und geneigte Fensterbank.\n'
               'Eingangsrahmen lässt die bestehende Türöffnung frei.\n'
               'Alle Bauteile gehören zu vorhandenen Raum-/Öffnungs-IDs.\n\n'
               'Glas ist eine dünne Alpha-Approximation;\n'
               'Innenräume sind leer und generisch.\n'
               'Raumprogramme, Nachtlicht und native LODs bleiben offen.',fontsize=11,va='top',linespacing=1.65)
    fig.suptitle(label+' · Öffnungsbauteile am konstruktiven Gebäude',fontsize=17,y=.97)
    fig.text(.055,.932,'Ringprofile: 88 Dreiecke/Fenster, 36/Eingang · Referenz zeigt vollständige Details',fontsize=12)
    fig.text(.055,.06,'Vorhandene Grundrisse, Höhen, Räume und Durchbrüche erhalten. Ergänzte Rahmen/Banken/Glasdarstellung sind prozedural; keine fertige Architekturfamilie.',fontsize=9)
    fig.savefig(output,dpi=100);plt.close(fig)
