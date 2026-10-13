"""Render source footprints, scoped POIs and inferred recipe families as a standalone plot."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Patch, PathPatch
from matplotlib.path import Path as PlotPath
import numpy as np
import shapely

from evidence import revision
from massing import polygons_of
from recipes import choose
from samples import read_site

COLOURS = {'residential':'#97bba7', 'commercial':'#dcb077', 'industrial':'#969bb4', 'institutional':'#8cb5c7',
           'garage':'#b6aba0', 'religious':'#b8a0c2', 'utility':'#6a747a', 'mixed-use':'#d69686', 'unknown':'#deddd6'}


def patch_of(polygon, colour):
    vertices, codes = [], []
    for ring in polygons_of(polygon)[0]:
        vertices.extend([*ring, ring[0]])
        codes.extend([PlotPath.MOVETO, *([PlotPath.LINETO]*(len(ring)-1)), PlotPath.CLOSEPOLY])
    return PathPatch(PlotPath(np.array(vertices), codes), facecolor=colour, edgecolor='#50595c', lw=.35)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site', default='Vienna')
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/context'))
    args = parser.parse_args()
    samples, receipt = read_site(args.site, context=True)
    chosen = [s for s in samples if s['polygon'].distance(shapely.Point(0, 0)) < 180]
    figure, axes = plt.subplots(figsize=(12.8, 7.2), dpi=100)
    figure.patch.set_facecolor('#faf9f6'); axes.set_facecolor('#faf9f6')
    families, dots = Counter(), []
    for sample in chosen:
        properties, polygon = sample['properties'], sample['polygon']
        if properties['render_height'] <= properties['render_min_height']:
            continue
        recipe = choose(properties, sample['context'], polygon, properties['render_height'], properties['render_min_height'])
        families[recipe['family']] += 1
        axes.add_patch(patch_of(polygon, COLOURS[recipe['family']]))
        for row in sample['context']['evidence']:
            if row.get('role') in ('retail', 'food', 'office', 'lodging', 'education', 'health', 'civic', 'culture'):
                dots.append(row['position_xy'])
    if dots:
        xy = np.array(dots)
        axes.scatter(xy[:, 0], xy[:, 1], s=10, c='#222d32', marker='o', zorder=5, label='lokaler POI; keine Gesamtgebaudenutzung')
    bounds = np.array([s['polygon'].bounds for s in chosen])
    low, high = bounds[:, :2].min(0), bounds[:, 2:].max(0)
    centre, radius = (low+high)/2, max(high-low)*.55
    axes.set(xlim=(centre[0]-radius, centre[0]+radius), ylim=(centre[1]-radius, centre[1]+radius),
             aspect='equal', xlabel='Ost [m]', ylabel='Nord [m]')
    axes.set_title(f'{args.site} · OSM-Grundrisse + Nutzungskontext', loc='left', fontsize=17, pad=18)
    legend = [Patch(facecolor=COLOURS[key], edgecolor='#50595c', label=f'{key}: {count}') for key, count in families.items()]
    axes.legend(handles=legend, loc='upper left', bbox_to_anchor=(1.02, 1.), frameon=False)
    axes.grid(alpha=.1)
    figure.subplots_adjust(left=.08, right=.77, top=.87, bottom=.16)
    figure.text(.08, .05, 'Farben: abgeleitete Rezeptfamilien, keine gelieferten Wandfarben. Punkte: eindeutig zugeordnete Teilnutzungen.\n'
                        'Eine gepinnte Kachel: Ausschnitt und Quellabdeckung sind begrenzt. Grundrisse und Höfe bleiben erhalten.', fontsize=10, color='#50595c')
    args.output.mkdir(parents=True, exist_ok=True)
    stem = args.site.lower()+'-context-map'
    image = args.output/(stem+'.png')
    figure.savefig(image); plt.close(figure)
    report = dict(**revision(), profile=[1280, 720], source_products=len(chosen), admitted_products=sum(families.values()),
                  renderer=dict(renderer='matplotlib Agg', status='complete'), representation='context-diagnostic',
                  families=dict(families), partial_poi_uses=len(dots), source_sha256=receipt['receipt']['sha256'],
                  image_sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
                  limitations=['diagnostic map, not runtime rendering', 'context-derived use is not a supplied building type'])
    (args.output/(stem+'-report.json')).write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(image=str(image), families=dict(families), partial_poi_uses=len(dots))))


if __name__ == '__main__':
    main()
