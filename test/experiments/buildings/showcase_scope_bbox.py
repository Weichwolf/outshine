"""Projected boxes, actual ridge roofs and costs of stationary-geometry alignment."""

import argparse
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely
from shapely import affinity

from building_plan_views import patch
from evidence import revision
from esri_oracle import CASES
from outshine_ref.buildings.modelling import Shape, cga_to_enu
from provider_contract import attributes
from samples import read_site
from showcase_groups import save_plot
from showcase_roof_ridge import mass, top_mesh
from showcase_roof_shed import spatial


def examples():
    rectangle = affinity.rotate(shapely.box(0, 0, 12, 5), 31, origin=(0, 0))
    elbow = affinity.rotate(shapely.Polygon([(0, 0), (10, 0), (10, 3), (5, 3), (5, 8), (0, 8)]), -23)
    _, site, identifier = CASES[1]
    sample = next(row for row in read_site(site)[0] if row['id'] == identifier)
    data = attributes(sample['properties'])
    rise = min(3., data['bodyHeight']*.25)
    return [('Rechteck · 31°', rectangle, 0., 3., 2.5, {}), ('L-Form · −23°', elbow, 0., 3., 2.5, {}),
            ('Wien · Hofgrundriss', sample['polygon'], data['minHeight'], data['buildingHeight']-rise, rise,
             dict(source_id=sample['id'], source_sha256=sample['source_sha256'], source_properties=sample['properties'],
                  source_holes=len(sample['polygon'].interiors), provider_height_interval_m=[data['minHeight'], data['buildingHeight']]))]


def box(scope):
    corners = np.array([[0., 0., 0.], [1., 0., 0.], [1., 0., 1.], [0., 0., 1.], [0., 0., 0.]])
    return cga_to_enu(corners @ scope.matrix[:3, :3].T+scope.origin)


def plan(output, provenance, rows):
    start = time.perf_counter()
    fig, axes = plt.subplots(2, 3, figsize=(38.4, 21.6))
    fig.suptitle('alignScopeToGeometryBBox("xz") · fester Grundriss, passende Längsachse', fontsize=32)
    for column, (name, polygon, _, eave, rise, _) in enumerate(rows):
        source = Shape.from_polygon(polygon, eave)
        fitted = source.align_scope_to_geometry_bbox('xz')
        before, after = box(source.scope), box(fitted.scope)
        extent = np.concatenate([before, after]); low, high = extent.min(axis=0), extent.max(axis=0)
        margin = max(high[:2]-low[:2])*.08
        axis = axes[0, column]
        patch(axis, polygon, facecolor='#cae7ee', edgecolor='#263544', linewidth=3.)
        axis.plot(before[:, 0], before[:, 1], '--', color='#83929b', linewidth=3., label='ursprünglicher Scope')
        axis.plot(after[:, 0], after[:, 1], color='#ed8855', linewidth=4., label='minimale projizierte Box')
        origin = cga_to_enu(fitted.scope.origin)
        direction = cga_to_enu(fitted.scope.axes[:, 0])*fitted.scope.size[0]*.35
        axis.annotate('', xy=origin[:2]+direction[:2], xytext=origin[:2], arrowprops=dict(arrowstyle='->', color='#ce3234', lw=4))
        old_area = source.scope.size[0]*source.scope.size[2]; area = fitted.scope.size[0]*fitted.scope.size[2]
        axis.set_title(f'{name}\nBoxfläche {old_area:.2f} → {area:.2f} m²', fontsize=25)
        axis.legend(fontsize=18); axis.set_xlim(low[0]-margin, high[0]+margin); axis.set_ylim(low[1]-margin, high[1]+margin)
        roof = fitted.roof_ridge(rise, 'byHeight'); mesh = top_mesh(roof); points = mesh['vertices']
        axis = axes[1, column]
        colour = axis.tricontourf(points[:, 0], points[:, 1], mesh['faces'], points[:, 2]-eave,
                                 levels=np.linspace(0., rise, 257), cmap='viridis')
        patch(axis, polygon, facecolor='none', edgecolor='#263544', linewidth=3.)
        fig.colorbar(colour, ax=axis, shrink=.65, label='Höhe über Traufe [m]')
        axis.set_title(f'First entlang der neuen Scope-X-Achse\nAnstieg {rise:.2f} m · Höfe bleiben offen', fontsize=25)
        for axis in axes[:, column]:
            axis.set(aspect='equal', xlabel='Ost [m]', ylabel='Nord [m]'); axis.grid(alpha=.15); axis.tick_params(labelsize=17)
    fig.text(.045, .02, 'Oben: dieselbe Geometrie, neue Box und X-Achse. Unten: wirklich erzeugte Dachhöhen. Keine Rekonstruktion eines belegten Daches.', fontsize=23)
    fig.tight_layout(rect=(0., .055, 1., .95), h_pad=5., w_pad=4.)
    return save_plot(fig, output, 'current-bbox-plan', 'Scope-Ausrichtung · minimale Box und Dachachsen',
                     'Rechteck, L-Form und gepinnter Wiener Hof: Geometrie bleibt stationär. roof_ridge verwendet die neue Längsachse; Dachwahl ist eine experimentelle Ergänzung.',
                     provenance, start, [3840, 2160], label='scope-bbox')


def pair(output, provenance, row, stem):
    name, polygon, base, eave, rise, metadata = row
    source = Shape.from_polygon(polygon, eave)
    aligned = source.align_scope_to_geometry_bbox('xz')
    span = polygon.bounds[2]-polygon.bounds[0]
    shapes = []
    for index, shape in enumerate((source, aligned)):
        roof = shape.roof_ridge(rise, 'byHeight')
        shapes.extend((part.translate('rel', 'world', index*(span+5.), 0., 0.), colour)
                      for part, colour in mass(polygon, base, eave, roof))
    return spatial(output, provenance, stem, name+' · vor und nach Scope-Ausrichtung', shapes, metadata,
                   'Links First entlang Ost, rechts entlang der flächenminimalen Scope-X-Achse. Identischer Grundriss und Höhenrahmen, native Dreiecke. Prüfpalette; Fassaden, Räume und vollständiges Rezept fehlen.', label='scope-bbox')


def measurements(output, provenance, verification):
    start = time.perf_counter(); rows = verification['measurements']
    fig, axes = plt.subplots(1, 2, figsize=(19.2, 10.8))
    labels = [row['source_id'].split(':')[0]+' · '+str(row['points'])+' Punkte' for row in rows]
    for index, (key, label, colour) in enumerate((('geos', 'GEOS · minimale Fläche', '#28738a'),
                                                ('hull_edges', 'alle Hull-Kanten · minimale Fläche', '#cb9252'),
                                                ('pca', 'PCA · Näherung', '#8f879a'))):
        axes[0].barh(np.arange(len(rows))+index*.23-.23, [row[key]['p50_ms'] for row in rows], height=.22, color=colour, label=label)
    axes[0].set(yticks=np.arange(len(rows)), yticklabels=labels, xlabel='p50 Richtungswahl [ms]')
    axes[0].legend(fontsize=12); axes[0].grid(axis='x', alpha=.2)
    axes[1].barh(labels, [(row['pca_area_m2']/row['minimum_area_m2']-1)*100 for row in rows], color='#8f879a')
    axes[1].set(xlabel='Zusätzliche Boxfläche durch PCA [%]'); axes[1].grid(axis='x', alpha=.2)
    fig.suptitle('Gleiche reale Eingaben · Zeit und geometrische Genauigkeit getrennt', fontsize=22)
    fig.text(.045, .04, '21 wechselnde warme Läufe. Hull-Vorbereitung enthalten; Shape-Umbau, Decode, Rendering und GPU ausgeschlossen.\nPCA erfüllt den Mindestflächenvertrag nicht. Rechnerwerte sind kein Frame-/Gerätenachweis.', fontsize=13)
    fig.tight_layout(rect=(0., .13, 1., .93), w_pad=3.)
    return save_plot(fig, output, 'current-bbox-costs', 'Scope-Box · Verfahrensvergleich auf realen Quellen',
                     'GEOS und vollständige Hull-Kantenprüfung erfüllen dieselbe Mindestfläche. PCA ist eine gemessene Näherung. Kosten und zusätzliche Fläche werden getrennt dargestellt.',
                     provenance, start, [1920, 1080], label='scope-bbox')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/bbox-release'))
    args = parser.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    provenance = revision(); rows = examples()
    plan(args.output, provenance, rows)
    pair(args.output, provenance, rows[0], 'current-bbox-rectangle')
    pair(args.output, provenance, rows[2], 'current-bbox-vienna')
    measurements(args.output, provenance, json.loads(Path('build/experiments/buildings/scope-bbox-verification.json').read_text()))
