#!/usr/bin/env python3
"""Solve complete cached OSM windows in 2D and expose remaining 2.5D work.

No downloads, height solving, native integration or GPU claims. All supplied tags
and source receipts are retained. Round end closures are an explicit first recipe,
not a claim that traffic connectivity, curvature or portals are already solved.
"""

import argparse
from collections import Counter
import json
import hashlib
from pathlib import Path
import time

import numpy as np
import shapely

from infrastructure_building_clearance import load_scene, plot_geometry, plot_structures, street_kind
from infrastructure_network_plan import bands_of, clustered_plan, global_plan, audit_plan, mesh_of, measure, verify
from infrastructure_network_graph import axes_of, verify as verify_graph


def source_roads(transport, recipes=None, widths=None):
    roads, seen, unclassified, routes, inferred = [], set(), Counter(), Counter(), Counter()
    recipes, widths = recipes or {}, widths or {}
    for source in transport:
        props = source['properties']
        tier = (int(props.get('layer', 0)), str(props.get('brunnel', 'ground')))
        identity = (shapely.normalize(source['line']).wkb,
                    tuple(sorted((k, str(v)) for k, v in props.items())))
        if identity in seen:
            continue
        seen.add(identity)
        kind = street_kind(props)
        width = source['width']
        recipe = recipes.get('axisRecipes', {}).get(kind, {})
        if recipe.get('role') == 'route':
            routes[kind] += 1
            continue
        if width is None:
            width = recipe.get('widthM', widths.get(recipe.get('widthFrom')))
            suffix = recipes.get('constructionSuffix')
            if suffix and kind.endswith(suffix):
                base = kind[:-len(suffix)]
                width = widths.get(recipes.get('constructionAliases', {}).get(base, base))
            if width is None:
                unclassified[kind] += 1
                continue
            inferred[kind] += 1
        road = dict(**source, tier=tier)
        road['width'] = width
        roads.append(road)
    return roads, dict(exact_duplicates=len(transport) - len(seen),
                       unclassified=dict(unclassified), routes=dict(routes), inferred_widths=dict(inferred),
                       unclassified_scope='Retained in input/plots; not yet a surface recipe.')


def colour(tier):
    if tier[1] == 'bridge':
        return '#167dba'
    if tier[1] == 'tunnel':
        return '#a43896'
    return '#d17318' if tier[0] else '#879594'


def render(output, place, roads, bands, plan, transport, view):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.lines import Line2D
    fig, axes = plt.subplots(1, 2, figsize=(14, 7), constrained_layout=True)
    for ax, title in zip(axes, ('Independent source bands', 'One surface per physical tier')):
        ax.set(xlim=(-view, view), ylim=(-view, view), aspect='equal', title=title,
               xlabel='East [m]', ylabel='North [m]', facecolor='#f1f0e8')
        ax.grid(alpha=.15)
    for road, band in zip(roads, bands):
        plot_geometry(axes[0], [band], colour(road['tier']))
        xy = shapely.get_coordinates(band.boundary)
        axes[0].plot(xy[:, 0], xy[:, 1], color='#263a35', linewidth=.25, alpha=.6)
    for tier, shape in sorted(plan.items()):
        plot_geometry(axes[1], [shape], colour(tier))
    for ax in axes:
        plot_structures(ax, transport)
    axes[1].legend(handles=[Line2D([], [], color='#167dba', label='Bridge'),
                            Line2D([], [], color='#a43896', linestyle='--', label='Tunnel'),
                            Line2D([], [], color='#d17318', linestyle=':', label='Other layer')])
    fig.suptitle(f'{place}: all source bands in the window; height/connectivity not yet solved')
    path = output / '2d' / f'{place}-network.png'
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=140)
    plt.close(fig)
    return str(path)


def save_inputs(path, receipt, transport):
    path.write_text(json.dumps(dict(receipt=receipt, roads=[dict(
        coordinates=list(road['line'].coords), width_m=road['width'],
        properties=road['properties']) for road in transport]), separators=(',', ':')) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--endpoint', required=True)
    parser.add_argument('--places', type=Path, default=Path('src/assets/places'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--radius', type=float, default=1600)
    parser.add_argument('--view', type=float, default=600)
    parser.add_argument('--precision', type=float, default=.001)
    parser.add_argument('--recipes', type=Path, default=Path(__file__).with_name('infrastructure_network_recipes.json'))
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('names', nargs='+')
    args = parser.parse_args()
    if not all(np.isfinite(v) and v > 0 for v in (args.radius, args.view, args.precision)) or args.repeats < 1:
        parser.error('positive finite dimensions and repeats required')
    args.side_room, args.native_plan = 0, None
    args.output.mkdir(parents=True, exist_ok=True)
    verify()
    verify_graph()
    templates = json.loads(Path('src/assets/world/vegetation.json').read_text())['templates']
    rules = [r for t in templates for r in t.get('osm', [])
             if r.get('layer') == 'streets' and r.get('widthM', 0) > 0]
    widths = {r['kind']: r['widthM'] for r in sorted(rules, key=lambda r: r.get('rank', 0))}
    recipes = json.loads(args.recipes.read_text())
    implementation = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in
                      (Path(__file__), Path(__file__).with_name('infrastructure_network_plan.py'),
                       Path(__file__).with_name('infrastructure_network_graph.py'), args.recipes)}
    reports = []
    for place in args.names:
        began = time.perf_counter()
        _, _, transport, receipt = load_scene(args, place, widths)
        decode_ms = (time.perf_counter() - began) * 1000
        save_inputs(args.output / f'{place}-input.json', receipt, transport)
        roads, normalization = source_roads(transport, recipes, widths)
        (_, graph_stats, transitions), graph_time = measure(lambda: axes_of(roads, args.precision), 1)
        bands, buffer_time = measure(lambda: bands_of(roads, args.precision), args.repeats)
        reference, global_time = measure(lambda: global_plan(roads, bands), args.repeats)
        (plan, groups), local_time = measure(lambda: clustered_plan(roads, bands), args.repeats)
        audit = audit_plan(bands, plan, reference)
        (meshes, mesh_stats), mesh_time = measure(lambda: mesh_of(plan), 1)
        report = dict(place=place, scope='Complete nine-tile cached window intersecting a bounded query; '
                      'horizontal surfaces only, no solved traffic or heights.',
                      radius_m=args.radius, precision_m=args.precision,
                      implementation_sha256=implementation,
                      source=receipt, normalization=normalization, decoded_source_ms=decode_ms,
                      source_parts=len(transport), planned_parts=len(roads), groups=groups,
                      axis_graphs=graph_stats, transitions=transitions, graph_planning=graph_time,
                      buffer=buffer_time, global_union=global_time, clustered_union=local_time,
                      triangulation_and_audit=mesh_time, audit=audit, meshes=mesh_stats)
        report['image'] = render(args.output, place, roads, bands, plan, transport, args.view)
        np.savez_compressed(args.output / f'{place}-mesh.npz', **{
            f'{i}_{field}': data for i, mesh in enumerate(meshes.values()) for field, data in mesh.items()})
        reports.append(report)
        (args.output / 'network.json').write_text(json.dumps(reports, indent=2) + '\n')
        print(json.dumps(dict(place=place, source=len(transport), planned=len(roads),
                              union_ms=round(local_time['median_ms'], 3),
                              global_ms=round(global_time['median_ms'], 3),
                              removed_m2=round(audit['duplicate_surface_area_removed_m2'], 3),
                              triangles=sum(m['triangles'] for m in mesh_stats),
                              unclassified=normalization['unclassified'])), flush=True)


if __name__ == '__main__':
    main()
