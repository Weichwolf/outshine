"""Metric skeleton partitions and actual hip-roof caps, before source-UV integration."""

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
from verify_skeleton_plan import caps


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
        mesh = engine_mesh(caps(plan, rise).emit()); points = mesh['vertices']
        selected = mesh['parts'] < len(plan.facets); triangles = mesh['faces'][selected]
        axis = axes[1, column]
        colour = axis.tricontourf(points[:, 0], points[:, 1], triangles, points[:, 2], levels=np.linspace(0., rise, 257), cmap='viridis')
        patch(axis, polygon, facecolor='none', edgecolor='#17272e', linewidth=3.)
        fig.colorbar(colour, ax=axis, shrink=.65, label='Höhe über Traufe [m]')
        axis.set_title(f'byHeight · {rise:.2f} m\nGeschlossene native Dachkappen, offene Höfe', fontsize=24)
        for axis in axes[:, column]:
            axis.set(aspect='equal', xlabel='Ost [m]', ylabel='Nord [m]'); axis.grid(alpha=.15); axis.tick_params(labelsize=17)
    fig.text(.04, .02, 'Unveränderliche metrische Pläne vor Ausgabe. Rot: gemeinsame Grate. Dachform ist eine prozedurale Ergänzung; kein vollständiges Architekturrezept.', fontsize=23)
    fig.tight_layout(rect=(0., .055, 1., .95), h_pad=5., w_pad=4.)
    return save_plot(fig, output, 'current-hip-plan', 'Walmdächer · Facetten und tatsächliche Höhen',
                     'L-Form, asymmetrisches Trapez und echter Wiener Innenhof. Die Planflächen erhalten alle ursprünglichen Quellkanten; keine Box als Ersatzgrundriss.',
                     provenance, begin, [3840, 2160], label='hip-plan')


def mass(row):
    _, polygon, base, eave, rise, _ = row
    plan = prepare_hip(polygon)
    body = Shape.from_polygon(polygon, base).extrude(eave-base)
    roof = caps(plan, rise).translate('rel', 'world', 0., eave, 0.)
    shapes = [(part, 0) for part in body.comp('f', Branch(~tagged('extrude.top')), alignment='noAlign')]
    shapes += [(part, 3) for part in roof.comp('f', Branch(tagged('roof.top')), alignment='noAlign')]
    return plan, shapes


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
                               'Gepinnter Grundriss und unverändertes Anbieter-Höhenintervall. Experimentelles Walmdach mit einer Facette pro Quellkante. Unveränderlicher Plan und native Kappen; Quell-UV0–9, Fassaden, Räume und vollständiges Rezept fehlen.',
                               label='hip-plan', direction=(1., -1.3, 2.5)))
    return reports


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/hip-plan-release'))
    args = parser.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    provenance = revision(); rows = examples()
    reports = [plan_image(args.output, provenance, rows), *views(args.output, provenance, rows)]
    print(json.dumps(dict(reports=reports)))
