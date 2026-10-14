"""Actual repaired edges and native patch geometry of the fixed-field inset alternative."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import shapely

from compare_roof_oracle import cases
from evidence import revision
from outshine_ref.buildings.modelling import Shape
from showcase_groups import save_plot
from showcase_hip_overhang import draw
from showcase_roof_shed import spatial
from showcase_scene import fitted_points


def plan_view(data, output, provenance):
    begin = time.perf_counter()
    fig, axes = plt.subplots(2, 2, figsize=(38.4, 21.6))
    fig.suptitle('Dachüberhang · lokale Konturreparatur gegen SDK', fontsize=32)
    choices = [('courtyard', 3., False), ('Vienna:112890:145', .5, True)]
    for column, (identifier, overhang, even) in enumerate(choices):
        row = next(r for r in data['results'] if r['id'] == identifier and r['mode'] == 'byAngle'
                   and r['overhang_m'] == overhang and r['even'] == even)
        sdk = row['sdk_roof_geometry']
        for level, (label, points, faces) in enumerate((
                ('Vor Reparatur', row['input_positions_m'], row['input_faces']),
                ('Nach eigener Reparatur', row['own_positions_m'], row['own_faces']))):
            axis = axes[level, column]
            draw(axis, np.asarray(sdk['positions_m']), sdk['faces'], '#de8821', 'solid', 'SDK-Dachkanten')
            draw(axis, np.asarray(points), faces, '#12659b', 'dashed', 'Eigene Dachkanten')
            if column:
                axis.set(xlim=(1178.7, 1182.7), ylim=(-543.2, -539.2))
            axis.set(aspect='equal', xlabel='Ost [m]', ylabel='Nord [m]')
            axis.set_title(f'{identifier} · {label}\n'
                           f"{row['repairs']} lokale Schnitte · finaler Kantenabstand {row['boundary_error_m']:.3g} m", fontsize=23)
            axis.tick_params(labelsize=17)
            axis.grid(alpha=.15)
            axis.legend(fontsize=16)
    fig.text(.035, .03, '417/444 gesamte Dachvergleiche bestätigt; 444/444 lokale Helfervergleiche. '
             '16 ungültige Projektionen und elf weitere Abweichungen bleiben. Positive Überhänge weiter offen.', fontsize=22)
    fig.tight_layout(rect=(0., .07, 1., .94), h_pad=4., w_pad=3.)
    return save_plot(fig, output, 'current-hip-repair-plan', 'Dachüberhang · Konturreparatur',
                     'Echte Knoten/Kanten vor und nach der eigenen lokalen Reparatur. Hof und Wiener Traufdetail; '
                     'Reparatur entspricht dem isolierten SDK-Helfer, beweist aber keine gültigen vollständigen Dachflächen.',
                     provenance, begin, [3840, 2160], label='hip-overhang-research')


def inset_view(output, provenance):
    row = next(r for r in cases() if r['id'] == 'Vienna:112890:45')
    polygon = row['polygon']
    body = Shape.from_polygon(polygon).extrude(4.)
    roof = Shape.from_polygon(polygon, 4.).roof_hip(3., 'byHeight')
    top = tuple(f for f in roof.geometry.polygons if 'roof.top' in f.tags)
    before = [(body, 0), (Shape.from_faces(roof.emit()['vertices'], top), 3)]
    closed = Shape.from_polygon(polygon,4.).roof_hip(3.,'byHeight',overhang=-.5)
    prepared = closed.roof_info.insets[0]
    after = [(body, 0),(closed,3)]
    spacing = polygon.bounds[2]-polygon.bounds[0]+10.
    shapes = before+[(s.translate('rel', 'world', spacing, 0., 0.), c) for s, c in after]
    metadata = dict(source_id=row['id'], source_sha256=row['source_sha256'], overhang_m=-.5,
                    eave_level_m=prepared.eave_level_m, nominal_roof_height_m=3., inset_patches=len(prepared.facets),
                    geometry_scope='Actual installed Shape.roof_hip negative inset: source UV0–9, shared geometric roots, bottom and rims; no SDK contour equivalence.')
    overview = spatial(output, provenance, 'current-hip-inset-vienna',
                   'Wien · vollständiges Dach und gültiger Höhenschnitt', shapes, metadata,
                   'Links Standarddach, rechts derselbe feste Dachhöhenverlauf oberhalb der neuen Traufhöhe. '
                   'Geschlossene eigene negative Variante mit ursprünglicher Unterseite und senkrechten Rändern; '
                   'einfache Prüfkörper, keine Architekturrezepte oder SDK-Konturgleichheit.',
                   label='hip-overhang-research', direction=(1., -1.3, 1.8))
    ring = np.asarray(shapely.orient_polygons(polygon).exterior.coords)[:-1]
    edges = np.roll(ring, -1, axis=0)-ring
    longest = int(np.argmax(np.linalg.norm(edges, axis=1)))
    centre = ring[longest]+edges[longest]/2
    edge = edges[longest]/np.linalg.norm(edges[longest])
    window = np.array([[centre[0]+x, centre[1]+y, z] for x in (-3., 3.) for y in (-3., 3.) for z in (2.5, 5.5)])
    camera = fitted_points(window, [1920, 1080], direction=(edge[1], -edge[0], 1.4))
    detail = spatial(output, provenance, 'current-hip-inset-detail', 'Traufdetail · eigenständiger Höhenschnitt',
                     after, metadata | dict(inspection_window_m=[6., 6., 3.]),
                     'Nahansicht des neuen Dachrands: 6×6 m um eine echte Traufkante. '
                     'Gold: tatsächliche geschlossene Dachhülle einschließlich Rand; Blau: unveränderter Prüfkörper. '
                     'Der neue Rand reicht bis zum ursprünglichen Quellniveau und schließt die frühere offene Fuge.',
                     label='hip-overhang-research', camera=camera)
    return [overview, detail]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/hip-repair-release'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    comparison = Path('build/experiments/buildings/hip-repair-comparison.json')
    data = json.loads(comparison.read_text())
    provenance = revision()
    reports = [plan_view(data, args.output, provenance), *inset_view(args.output, provenance)]
    for name in reports:
        path = args.output/name
        record = json.loads(path.read_text())
        record.update(comparison_sha256=hashlib.sha256(comparison.read_bytes()).hexdigest(),
                      comparison_commit=data['commit'], comparison_tree_dirty=data['experiment_tree_dirty'])
        path.write_text(json.dumps(record, ensure_ascii=False, indent=2)+'\n')
    print(reports)
