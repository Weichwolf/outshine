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
from demand import View
from evidence import revision
from hip_exterior_mesh_probe import emit
from modelling_scope_probe import engine_mesh
from outshine_ref.buildings.modelling import Shape
from showcase_groups import save_plot
from showcase_roof_shed import spatial
from showcase_scene import fitted_points
from verify_modelling_roof_shed import source


def camera_for(shapes):
    points=[]
    for shape,_ in shapes:
        mesh=engine_mesh(shape.emit());points.append(mesh['vertices'][mesh['faces']].reshape(-1,3))
    points=np.concatenate(points);centre=(points.min(axis=0)+points.max(axis=0))/2
    direction=np.array([1.,-1.2,1.5]);direction/=np.linalg.norm(direction)
    basis=View(centre+direction,centre,width=1920,height=1080)
    projected=(points-centre)@basis.basis.T
    required=np.maximum(abs(projected[:,0])*basis.focal_px/(1920*.42),
                        abs(projected[:,1])*basis.focal_px/(1080*.40))-projected[:,2]
    return View(centre+direction*(required.max()+1.),centre,width=1920,height=1080)


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
            label='hip-exterior',camera=camera_for(shapes)))
        if not row['control']:
            centre=proof.plan.origin+np.array([30.6646967685,-9.5869494139])
            window=np.array([[centre[0]+x,centre[1]+y,z] for x in (-2.5,2.5) for y in (-2.5,2.5) for z in (3.5,6.5)])
            camera=fitted_points(window,[1920,1080],direction=(1.,-1.2,.8))
            reports.append(spatial(output,provenance,'current-hip-exterior-tokyo-detail',
                'Tokyo · Traufdetail am bisherigen Gegenfall',shapes,metadata|dict(inspection_window_m=[5.,5.,3.]),
                '5×5-m-Nahansicht am früheren 5,6-cm-Höhensprung der Streifenvariante. '
                'Die triangulierte Dachschürze besitzt gemeinsame Eckhöhen und einen geschlossenen Traufrand. '
                'Eigene experimentelle Geometrie; Prüfpalette und unveränderter Quellkörper.',label='hip-exterior',camera=camera))
    return reports


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/hip-exterior-release'))
    print(render(parser.parse_args().output))
