"""Show successful eave extensions and retained SDK counterexamples in plan view."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import matplotlib
matplotlib.use('Agg')
from matplotlib.collections import LineCollection
import matplotlib.pyplot as plt
import numpy as np
from scipy.spatial import cKDTree

from building_plan_views import patch
from compare_roof_oracle import cases
from evidence import revision
from showcase_groups import save_plot


def draw(axis, points, faces, colour, style, label):
    lines = []
    for face in faces:
        ring = points[face, :2]
        lines.extend(np.stack((ring, np.roll(ring, -1, axis=0)), axis=1))
    axis.add_collection(LineCollection(lines, colors=colour, linewidths=1.8,
                                      linestyles=style, label=label))
    axis.autoscale()


def render(comparison, output):
    data = json.loads(comparison.read_text())
    provenance = revision()
    key = 'test/experiments/buildings/hip_overhang_probe.py'
    assert data['implementation_sha256'][key] == provenance['implementation_sha256'][key]
    sources = {r['id']: r['polygon'] for r in cases()}
    choices = [('rectangle', .5, False), ('trapezoid', .5, True),
               ('courtyard', 3., False), ('Vienna:112890:145', .5, True)]
    started = time.perf_counter()
    fig, axes = plt.subplots(2, 2, figsize=(38.4, 21.6))
    fig.suptitle('Dachüberhang · Versuch gegen ausführbare SDK-Geometrie', fontsize=32)
    for axis, (identifier, overhang, even) in zip(axes.flat, choices, strict=True):
        row = next(r for r in data['results'] if r['id'] == identifier and r['overhang_m'] == overhang
                   and r['even'] == even and r['mode'] == 'byAngle')
        sdk = row['sdk_geometry']
        patch(axis, sources[identifier], facecolor='#e6e9eb', edgecolor='#44535c', linewidth=2.)
        draw(axis, np.asarray(sdk['positions_m']), sdk['faces'], '#de8821', 'solid', 'SDK-Dachkanten')
        draw(axis, np.asarray(row['own_positions_m']), row['own_faces'], '#12659b', 'dashed', 'Eigene Traufverlängerung')
        axis.set_title(f"{identifier} · 25° · Überhang {overhang:g} m · even={even}\n"
                       f"{row['status']} · Knotenabstand {row['maximum_node_distance_m']:.6g} m", fontsize=23)
        axis.set(aspect='equal', xlabel='Ost [m]', ylabel='Nord [m]')
        axis.tick_params(labelsize=16)
        axis.grid(alpha=.15)
        axis.legend(fontsize=16)
        if identifier.startswith('Vienna:'):
            own = np.asarray(row['own_positions_m'])
            actual = np.asarray(sdk['positions_m'])
            errors, _ = cKDTree(actual).query(own)
            centre = own[np.argmax(errors), :2]
            inset = axis.inset_axes([.025, .61, .37, .34])
            draw(inset, actual, sdk['faces'], '#de8821', 'solid', 'SDK')
            draw(inset, own, row['own_faces'], '#12659b', 'dashed', 'Versuch')
            inset.scatter(*own[:, :2].T, s=18, color='#12659b')
            inset.scatter(*actual[:, :2].T, s=18, color='#de8821')
            inset.set(xlim=(centre[0]-2., centre[0]+2.), ylim=(centre[1]-2., centre[1]+2.),
                      aspect='equal', title='Traufdetail · 4×4 m')
            inset.tick_params(labelsize=10)
    fig.text(.035, .035, 'Oben: bestätigte einfache Formen. Unten: offen dokumentierte Gegenfälle. '
             'Traufen sinken ab; Firstknoten bleiben erhalten. Keine Shape-/UV-/Trim-Abnahme.', fontsize=22)
    fig.tight_layout(rect=(0., .07, 1., .94), h_pad=3., w_pad=3.)
    output.mkdir(parents=True, exist_ok=True)
    name = save_plot(fig, output, 'current-hip-overhang-oracle',
                     'Dachüberhänge · SDK-Vergleich und Gegenfälle',
                     'Experimentelle Traufverlängerung auf unveränderten Skeletten. '
                     'Erfolge und offene Kontur-/even-Abweichungen ausdrücklich zusammen dargestellt; '
                     'noch keine Bibliotheksoperation für Überhang oder fertiges Architekturrezept.',
                     provenance, started, [3840, 2160], label='hip-overhang-research')
    report = json.loads((output/name).read_text())
    report.update(comparison_sha256=hashlib.sha256(comparison.read_bytes()).hexdigest(),
                  comparison_counts=data['counts'], comparison_commit=data['commit'],
                  comparison_tree_dirty=data['experiment_tree_dirty'])
    (output/name).write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print(name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--comparison', type=Path, default=Path('build/experiments/buildings/hip-overhang-comparison.json'))
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/hip-overhang-release'))
    args = parser.parse_args()
    render(args.comparison, args.output)
