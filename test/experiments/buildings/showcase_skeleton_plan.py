"""Metric hip partitions, native source-UV interpolation and installed Shape roof geometry."""

import argparse
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from building_plan_views import patch
from evidence import revision
from esri_oracle import CASES
from modelling_scope_probe import engine_mesh
from outshine_ref.buildings.modelling import Branch, Shape, prepare_hip
from outshine_ref.buildings.modelling.selectors import tagged
from provider_contract import attributes
from samples import read_site
from showcase_groups import save_plot
from showcase_roof_shed import spatial
from verify_modelling_roof_shed import source


def examples():
    rectangle = shapely.box(0, 0, 12, 5)
    elbow = shapely.Polygon([(0, 0), (10, 0), (10, 3), (5, 3), (5, 8), (0, 8)])
    trapezoid = shapely.Polygon([(0, 0), (12, 0), (10, 7), (2, 6)])
    _, site, identifier = CASES[1]
    rows = [('Rechteck', rectangle, 0., 3., 3., {}), ('L-Form', elbow, 0., 3., 3., {}),
            ('Trapez · ungleiche Ridgehöhen', trapezoid, 0., 3., 3., {})]
    for label, identifier in [('Wien · offener Innenhof', identifier), ('Wien · schwieriger 45-Kanten-Grundriss', 'Vienna:1210000:12')]:
        sample = next(s for s in read_site(site)[0] if s['id'] == identifier)
        data = attributes(sample['properties']); rise = min(3., data['bodyHeight']*.25)
        metadata = dict(source_id=sample['id'], source_sha256=sample['source_sha256'], source_properties=sample['properties'],
                        source_holes=len(sample['polygon'].interiors), provider_height_interval_m=[data['minHeight'], data['buildingHeight']])
        rows.append((label, sample['polygon'], data['minHeight'], data['buildingHeight']-rise, rise, metadata))
    return rows


def plan_image(output, provenance, rows):
    begin = time.perf_counter(); fig, axes = plt.subplots(2, 3, figsize=(38.4, 21.6))
    fig.suptitle('Walmdach · tatsächliche Facetten, Grate und metrische Höhen', fontsize=33)
    for column, row in enumerate((rows[1], rows[2], rows[3])):
        label, polygon, _, _, rise, _ = row
        plan = prepare_hip(polygon); world = plan.positions_m+np.asarray(plan.origin)
        axis = axes[0, column]
        for index, facet in enumerate(plan.facets):
            patch(axis, shapely.affinity.translate(facet.polygon, *plan.origin), facecolor=plt.get_cmap('tab20')(index % 20), edgecolor='#45545d', linewidth=2.)
        patch(axis, polygon, facecolor='none', edgecolor='#17272e', linewidth=4.)
        shared = {}
        for facet in plan.facets:
            ids = facet.node_indices
            for a, b in zip(ids, ids[1:]+ids[:1]):
                shared.setdefault(tuple(sorted((a, b))), 0); shared[tuple(sorted((a, b)))]+=1
        for (a, b), count in shared.items():
            if count == 2:
                axis.plot(world[[a, b], 0], world[[a, b], 1], color='#ad3335', linewidth=3.)
        axis.set_title(label+f'\n{len(plan.facets)} Facetten · Quellkanten erhalten', fontsize=24)
        roof = source(polygon).roof_hip(rise, 'byHeight')
        mesh = top_mesh(roof); points = mesh['vertices']; triangles = mesh['faces']
        axis = axes[1, column]
        colour = axis.tricontourf(points[:, 0], points[:, 1], triangles, points[:, 2], levels=np.linspace(0., rise, 257), cmap='viridis')
        patch(axis, polygon, facecolor='none', edgecolor='#17272e', linewidth=3.)
        fig.colorbar(colour, ax=axis, shrink=.65, label='Höhe über Traufe [m]')
        axis.set_title(f'Shape.roof_hip · {rise:.2f} m\nNative Quell-Dreiecke und UV0–9 erhalten', fontsize=24)
        for axis in axes[:, column]:
            axis.set(aspect='equal', xlabel='Ost [m]', ylabel='Nord [m]'); axis.grid(alpha=.15); axis.tick_params(labelsize=17)
    fig.text(.04, .02, 'Rot: gemeinsame Grate. Höhen aus Shape.roof_hip und seinen echten nativen Dreiecken. Dachform ist eine prozedurale Ergänzung; kein vollständiges Architekturrezept.', fontsize=23)
    fig.tight_layout(rect=(0., .055, 1., .95), h_pad=5., w_pad=4.)
    return save_plot(fig, output, 'current-hip-plan', 'Walmdächer · Facetten und tatsächliche Höhen',
                     'L-Form, asymmetrisches Trapez und echter Wiener Innenhof. Die Planflächen erhalten alle ursprünglichen Quellkanten; keine Box als Ersatzgrundriss.',
                     provenance, begin, [3840, 2160], label='hip-plan')


def mass(row):
    _, polygon, base, eave, rise, _ = row
    body = Shape.from_polygon(polygon, base).extrude(eave-base)
    roof = source(polygon).translate('rel', 'world', 0., eave, 0.).roof_hip(rise, 'byHeight')
    shapes = [(part, 0) for part in body.comp('f', Branch(~tagged('extrude.top')), alignment='noAlign')]
    shapes += [(part, 3) for part in roof.comp('f', Branch(tagged('roof.top')), alignment='noAlign')]
    return roof.roof_info.plans[0], shapes


def top_mesh(roof):
    mesh = engine_mesh(roof.emit())
    selected = np.asarray(['roof.top' in face.tags for face in roof.geometry.polygons])[mesh['parts']]
    return mesh | dict(faces=mesh['faces'][selected])


def uv_image(output, provenance):
    begin = time.perf_counter(); original = source(shapely.box(0,0,12,5))
    before, after = engine_mesh(original.emit()), top_mesh(original.roof_hip(3., 'byHeight'))
    fig, axes = plt.subplots(2,2,figsize=(19.2,10.8))
    for component in (0,1):
        active = before['uv'][np.unique(before['faces']),component]; levels = np.linspace(active.min(),active.max(),257)
        for column,(title,mesh) in enumerate((('Native Quelle',before),('Walmdach · Rückprojektion',after))):
            axis = axes[component,column]; points = mesh['vertices']
            colour = axis.tricontourf(points[:,0],points[:,1],mesh['faces'],mesh['uv'][:,component],levels=levels,cmap='viridis')
            axis.triplot(points[:,0],points[:,1],mesh['faces'],color='#263b51',alpha=.45,linewidth=.8)
            fig.colorbar(colour,ax=axis,shrink=.7,ticks=np.linspace(levels[0],levels[-1],5))
            axis.set(aspect='equal',xlabel='Ost [m]',ylabel='Nord [m]');axis.set_title(title+' · '+('U' if component==0 else 'V'),fontsize=16)
    fig.suptitle('Shape.roof_hip · nichtaffine native UV-Flächen erhalten',fontsize=24)
    fig.text(.045,.02,'Quelle und Dach besitzen dieselbe UV-Fläche. Die Schnittkanten erhalten die ursprünglichen Interpolationsbereiche; UV0–9 sind unabhängig geprüft.',fontsize=14)
    fig.subplots_adjust(top=.88,bottom=.10,hspace=.28,wspace=.23)
    return save_plot(fig,output,'current-hip-uv','Walmdach · native UV-Flächen erhalten',
                     'Echte emittierte UV-Streams: Quell-Dreiecke und Rückprojektion des Walmdachs. Neue Gratpunkte interpolieren innerhalb der ursprünglichen nativen Dreiecke.',
                     provenance,begin,[1920,1080],label='hip-plan')


def views(output, provenance, rows):
    controls = []
    for index, row in enumerate(rows[:3]):
        _, shapes = mass(row)
        controls += [(shape.translate('rel', 'world', index*17., 0., 0.), colour) for shape, colour in shapes]
    reports = [spatial(output, provenance, 'current-hip-controls', 'Walmdächer · Rechteck, L und Trapez', controls, {},
                       'Native geschlossene Dachkappen über identischen Quellkörpern. Das Trapez besitzt natürliche ungleiche Ridgehöhen; keine even-Nachbearbeitung. Prüfpalette, keine fertige Rezeptfamilie.',
                       label='hip-plan', direction=(1., -1.3, 2.5))]
    for row, stem in zip(rows[3:], ('current-hip-vienna', 'current-hip-difficult'), strict=True):
        plan, shapes = mass(row)
        metadata = row[-1] | dict(skeleton_backend=plan.method, skeleton_fallback_reason=plan.fallback_reason,
                                  source_edges=len(plan.facets), experimental_roof_rise_m=row[4])
        reports.append(spatial(output, provenance, stem, row[0], shapes, metadata,
                               'Gepinnter Grundriss und unverändertes Anbieter-Höhenintervall. Shape.roof_hip erhält Quell-Dreiecksinterpolation und UV0–9. Opake Prüfpalette; Fassaden, Räume, Überhang/even und vollständiges Rezept fehlen.',
                               label='hip-plan', direction=(1., -1.3, 2.5)))
    return reports


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/hip-plan-release'))
    args = parser.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    provenance = revision(); rows = examples()
    reports = [plan_image(args.output, provenance, rows), uv_image(args.output, provenance), *views(args.output, provenance, rows)]
    print(json.dumps(dict(reports=reports)))
