"""Actual public roof products: positive collars, arbitrary frames and plan reuse."""

import argparse
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from compare_roof_oracle import cases
from evidence import revision
from outshine_ref.buildings.modelling import Shape,hip_footprints,prepare_hip
from showcase_groups import save_plot
from showcase_hip_exterior import camera_for
from showcase_roof_shed import spatial
from showcase_scene import fitted_points
from verify_modelling_roof_shed import source


def render(output):
    output.mkdir(parents=True,exist_ok=True);provenance=revision();started=time.perf_counter()
    polygon=shapely.Polygon([(0,0),(16.7,0),(16.7,12.3),(0,12.3)],
        [[(6.3816405375158,7.138778040313788),(5.117961890258449,5.588579254200913),
          (8.2183594624842,3.061221959686211),(9.48203810974155,4.611420745799086)]])
    authored=source(polygon);plans=tuple(prepare_hip(p) for p in hip_footprints(authored));shapes=[]
    for n,(height,even,overhang) in enumerate([(2.,False,.5),(3.,True,1.)]):
        roof=authored.roof_hip(height,'byHeight',even=even,overhang=overhang,prepared_plans=plans)
        shapes.extend([(Shape.from_polygon(polygon).extrude(5.).translate('rel','world',n*23.,0.,0.),0),
                       (roof.translate('rel','world',n*23.,5.,0.),3)])
    reports=[spatial(output,provenance,'current-positive-hip-reuse','Positive Überstände · zwei Produkte aus einem Plan',shapes,
        dict(api='Shape.roof_hip',prepared_plans=len(plans),shell_depth_m=.1,
             roof_parameters=[dict(height_m=2.,even=False,overhang_m=.5),dict(height_m=3.,even=True,overhang_m=1.)]),
        'Links Standard 2 m mit 0,5 m Dachüberstand, rechts even 3 m mit 1 m. Gleicher unveränderlicher Plan; '
        'tatsächliche geschlossene Hüllen und offene Höfe. Prüfpalette, noch keine Architektur-Rezeptfamilie.',
        label='positive-hip',camera=camera_for(shapes))]
    tilted=source(polygon).r(17.,31.,23.)
    roof=tilted.roof_hip(3.,'byHeight',overhang=.75,even=True)
    shapes=[(Shape.from_polygon(polygon).extrude(-2.).r(17.,31.,23.),0),(roof,3)]
    reports.append(spatial(output,provenance,'current-positive-hip-frame','Gedrehte Quellfläche · geschlossene Dachschürze',shapes,
        dict(api='Shape.roof_hip',source_rotation_deg=[17.,31.,23.],overhang_m=.75,shell_depth_m=.1),
        'Überstand und Boden folgen der ursprünglichen Flächennormale. Kein Abflachen in eine Welt-Horizontalebene; '
        'UV0–9 und Materialbesitz bleiben erhalten. Geometriekontrolle, kein Gebäuderezept.',
        label='positive-hip',camera=camera_for(shapes)))
    row=next(r for r in cases() if r['id']=='Tokyo:17970970:5259');authored=source(row['polygon'])
    roof=authored.roof_hip(3.,'byHeight',overhang=.5)
    shapes=[(Shape.from_polygon(row['polygon']).extrude(5.),0),(roof.translate('rel','world',0.,5.,0.),3)]
    centre=np.array([30.6646967685,-9.5869494139])+prepare_hip(row['polygon']).origin
    window=np.array([[centre[0]+x,centre[1]+y,z] for x in (-2.5,2.5) for y in (-2.5,2.5) for z in (3.5,6.5)])
    camera=fitted_points(window,[1920,1080],direction=(1.,-1.2,.8))
    reports.append(spatial(output,provenance,'current-positive-hip-tokyo-detail','Tokyo · Nahdetail des Bibliotheksprodukts',shapes,
        dict(api='Shape.roof_hip',source_id=row['id'],overhang_m=.5,shell_depth_m=.1,inspection_window_m=[5.,5.,3.]),
        '5×5-m-Nahansicht des realen Gegenfalls. Jetzt über die installierbare Shape-API erzeugt; '
        'kontinuierliche Außenflächen und geschlossener Traufrand. Keine SDK-Tessellierungsgleichheit.',
        label='positive-hip',camera=camera))
    fig,axes=plt.subplots(1,2,figsize=(38.4,21.6))
    fig.suptitle('Positive Dachschürze · ursprüngliches Feld und eigene Außenflächen',fontsize=32)
    for axis,result,title in zip(axes,[source(polygon).roof_hip(3.,'byHeight',overhang=.75),roof],
            ['Gedrehter Innenhof-Grundriss','Tokyo:17970970:5259'],strict=True):
        plan=result.roof_info.plans[0];surface=result.roof_info.exteriors[0]
        for patch in surface.patches:
            xy=np.asarray(patch.polygon.exterior.coords);axis.fill(*xy.T,color='#dfa446',edgecolor='#9c5d13',linewidth=.6)
        for ring in (plan.footprint.exterior,*plan.footprint.interiors):
            xy=np.asarray(ring.coords);axis.plot(*xy.T,color='#194c73',linewidth=3.)
        axis.set(aspect='equal',xlabel='Lokale Fläche X [m]',ylabel='Lokale Fläche Y [m]',title=title)
        axis.tick_params(labelsize=18);axis.grid(alpha=.12)
    fig.text(.035,.025,'Blau: ursprüngliche Traufe. Gold: tatsächliche Außenfacetten. CDT, Miter-Limit 5, '
             'Boden 10 cm unter äußerer Traufe; eigene Modellierpolitik.',fontsize=22)
    fig.tight_layout(rect=(0.,.07,1.,.94))
    reports.append(save_plot(fig,output,'current-positive-hip-plan','Positive Überstände · Bibliotheksdomänen',
        'Tatsächlich ausgegebene Außenfacetten in intrinsischen Flächenkoordinaten. Das ursprüngliche Dachfeld bleibt erhalten.',
        provenance,started,[3840,2160],label='positive-hip'))
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/positive-hip-release'))
    print(render(parser.parse_args().output))
