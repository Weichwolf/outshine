"""Current positive-overhang collar geometry, including the closed roof shell."""

import argparse
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

from compare_roof_oracle import cases
from evidence import revision
from hip_exterior_mesh_probe import emit
from outshine_ref.buildings.modelling import Shape
from showcase_groups import save_plot
from showcase_roof_shed import spatial
from verify_modelling_roof_shed import source


def render(output):
    output.mkdir(parents=True,exist_ok=True);provenance=revision();rows=cases()
    reports=[];started=time.perf_counter()
    fig,axes=plt.subplots(1,2,figsize=(38.4,21.6))
    fig.suptitle('Positiver Dachüberstand · kontinuierliche triangulierte Dachschürze',fontsize=32)
    examples=[]
    for axis,identifier,overhang in zip(axes,['courtyard','Tokyo:17970970:5259'],[.75,.5],strict=True):
        row=next(r for r in rows if r['id']==identifier)
        proof=emit(source(row['polygon']),row['polygon'],overhang=overhang,method='collar')
        surface=proof.surface;plan=proof.plan
        for patch in surface.patches:
            corners=np.asarray(patch.polygon.exterior.coords)
            axis.fill(*corners.T,color='#dfa446',edgecolor='#9c5d13',linewidth=.6)
        for ring in (plan.footprint.exterior,*plan.footprint.interiors):
            xy=np.asarray(ring.coords);axis.plot(*xy.T,color='#194c73',linewidth=3.)
        axis.set(aspect='equal',xlabel='Ost relativ zum Plan [m]',ylabel='Nord relativ zum Plan [m]')
        axis.set_title(f'{identifier}\n{overhang:g} m Dachlänge · {len(surface.patches)} Außenfacetten',fontsize=24)
        axis.grid(alpha=.12);axis.tick_params(labelsize=18)
        examples.append((row,proof))
    fig.text(.035,.025,'Blau: unveränderter Quellrand. Gold: tatsächliche Außenfacetten. Begrenzter Miter (5), '
             'gemeinsame Eckhöhen, Boden 10 cm unter äußerer Traufe. Eigene Variante; keine SDK-Gleichheit.',fontsize=22)
    fig.tight_layout(rect=(0.,.07,1.,.94))
    reports.append(save_plot(fig,output,'current-hip-exterior-plan','Dachüberstand · kontinuierliche Außenflächen',
        'Innenhof und realer Tokio-Gegenfall. Die Außenflächen werden gemeinsam trianguliert; das innere Dachfeld bleibt erhalten. '
        'Horizontale Quellflächen als Experiment, noch keine allgemeine Shape-API.',provenance,started,[3840,2160],label='hip-exterior'))
    for row,proof in examples:
        identifier=row['id'];roof=proof.shape.translate('rel','world',0.,5.,0.)
        shapes=[(Shape.from_polygon(row['polygon'],0.).extrude(5.),0),(roof,3)]
        stem='current-hip-exterior-'+('courtyard' if row['control'] else 'tokyo')
        metadata=dict(source_id=identifier,source_sha256=row.get('source_sha256'),
            geometry_scope='Own experimental positive collar and closed 10 cm offset bottom, horizontal source only',
            exterior_method='constrained Delaunay collar, miter limit 5',surface_facets=len(proof.surface.patches),
            roof_triangles=len(proof.shape.emit()['faces']),independent_field_volume_m3=proof.expected_volume_m3)
        reports.append(spatial(output,provenance,stem,
            ('Innenhof' if row['control'] else 'Tokyo')+' · geschlossene Dachhülle mit Überstand',shapes,metadata,
            'Tatsächliches geschlossenes Mesh: Blau zeigt den Quellkörper, Gold Dach und Traufrand. '
            'Die Dachschürze reicht über die ursprüngliche Traufe hinaus; das innere Dachfeld bleibt unverändert. '
            'Prüfpalette, keine fertige Architektur-Rezeptfamilie oder SDK-Tessellierungsgleichheit.',
            label='hip-exterior',direction=(1.,-1.2,1.5)))
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/hip-exterior-release'))
    print(render(parser.parse_args().output))
