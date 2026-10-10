#!/usr/bin/env python3
"""Offline 2D building/corridor tradeoff on cached OpenMapTiles, using GEOS.

Source bands approximate a future infrastructure plan; optional native Zurich axes
replace them. This experiment does not prove final height intervals, road profiles,
native cache integration or frame time. All thresholds are experimental parameters.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import time
import xml.etree.ElementTree as ET

import numpy as np
import shapely
from shapely.geometry import LineString, Polygon, box
from shapely.geometry.polygon import orient

from osm_provider_inventory import fields, packed, value_of

GIRTH_M = 40075016.68557849


def commands(data):
    values = iter(packed(data))
    x = y = 0
    parts, part = [], []
    for command in values:
        kind, count = command & 7, command >> 3
        if not count or kind not in (1, 2, 7):
            raise ValueError('invalid MVT geometry command')
        for _ in range(count):
            if kind == 7:
                if not part:
                    raise ValueError('empty MVT ring')
                part.append(part[0])
                continue
            dx, dy = next(values), next(values)
            x += (dx >> 1) ^ -(dx & 1)
            y += (dy >> 1) ^ -(dy & 1)
            if kind == 1 and part:
                parts.append(part)
                part = []
            part.append((x, y))
    if part:
        parts.append(part)
    return parts


def features(data):
    for number, _, payload in fields(data):
        if number != 3:
            continue
        layer = list(fields(payload))
        name = next(v.decode() for n, _, v in layer if n == 1)
        if name not in ('building', 'transportation'):
            continue
        extent = next((v for n, _, v in layer if n == 5), 4096)
        keys = [v.decode() for n, _, v in layer if n == 3]
        values = [value_of(v) for n, _, v in layer if n == 4]
        for n, _, payload in layer:
            if n != 2:
                continue
            feature = list(fields(payload))
            tags = [x for tag, _, v in feature if tag == 2 for x in packed(v)]
            if len(tags) % 2:
                raise ValueError('odd MVT tag list')
            props = {keys[tags[i]]: values[tags[i + 1]] for i in range(0, len(tags), 2)}
            kind = next((v for tag, _, v in feature if tag == 3), 0)
            geometry = b''.join(v for tag, _, v in feature if tag == 4)
            yield name, extent, kind, props, commands(geometry)


def polygon_parts(rings):
    shell, holes, sign = None, [], 0
    for ring in rings:
        ring = np.asarray(ring)
        area = np.sum(ring[:-1, 0] * ring[1:, 1] - ring[1:, 0] * ring[:-1, 1])
        if area == 0:
            raise ValueError('zero area MVT polygon ring')
        if shell is None or (area > 0) == (sign > 0):
            if shell is not None:
                yield Polygon(shell, holes)
            shell, holes, sign = ring, [], area
        else:
            holes.append(ring)
    if shell is not None:
        yield Polygon(shell, holes)


def source_tile(args, z, x, y):
    revision = args.endpoint.split('/planet/')[1].split('/')[0]
    subject = f'openfreemap.openmaptiles\n2\n{revision}\nvector\n{z}/{x}/{y}\n{args.endpoint}'
    path = args.sources / hashlib.sha256(subject.encode()).hexdigest()
    data = path.read_bytes()
    if len(data) > 4 * 1024**2:
        raise ValueError('MVT exceeds experiment byte limit')
    return data, dict(tile=[z, x, y], path=str(path), bytes=len(data),
                     sha256=hashlib.sha256(data).hexdigest())


def street_kind(props):
    kind = props.get('class', '')
    if kind in ('path', 'transit'):
        return props.get('subclass', kind)
    return 'residential' if kind == 'minor' else kind


def load_scene(args, place, widths):
    world = ET.parse(args.places / f'{place}.scenario').getroot().find('world')
    lat, lon = float(world.get('lat')), float(world.get('lon'))
    scale = math.cos(math.radians(lat))
    anchor = np.array([lon / 360 * GIRTH_M,
                       math.asinh(math.tan(math.radians(lat))) * GIRTH_M / (2 * math.pi)])
    z, n = 14, 1 << 14
    tx = int((lon + 180) / 360 * n)
    ty = int((1 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2 * n)
    span = GIRTH_M / n
    radius = box(-args.radius, -args.radius, args.radius, args.radius)
    buildings, roads, receipts, seen = [], [], [], set()
    elevated, invalid = 0, 0
    for x in range(tx - 1, tx + 2):
        for y in range(ty - 1, ty + 2):
            data, receipt = source_tile(args, z, x, y)
            receipts.append(receipt)
            for layer, extent, kind, props, parts in features(data):
                local = [((np.asarray(p) / extent + [x, y]) * [span, -span]
                          + [-GIRTH_M / 2, GIRTH_M / 2] - anchor) * scale for p in parts]
                if layer == 'building' and kind == 3:
                    if float(props.get('render_min_height', 0)) >= 5.7:
                        elevated += 1
                        continue
                    for poly in polygon_parts(local):
                        if not poly.is_valid:
                            invalid += 1
                            continue
                        key = shapely.normalize(poly).wkb
                        if poly.intersects(radius) and key not in seen:
                            buildings.append(poly)
                            seen.add(key)
                elif layer == 'transportation' and kind == 2:
                    if props.get('brunnel') in ('bridge', 'tunnel') or props.get('layer', 0) != 0:
                        continue
                    width = widths.get(street_kind(props))
                    if width is None:
                        continue
                    for part in local:
                        if len(part) >= 2:
                            poly = LineString(part).buffer(width * .5 + args.side_room,
                                                          resolution=2, cap_style=2)
                            if poly.intersects(radius):
                                roads.append(poly)
    plan = None
    if args.native_plan and place == 'ZuerichHauptbahnhof':
        data = args.native_plan.read_bytes()
        native = json.loads(data)
        roads = [LineString([(p['eastM'], p['northM']) for p in axis]).buffer(
            way['halfWidthM'] + args.side_room, resolution=2, cap_style=2)
            for way, axis in zip(native['ways'], native['designed'])
            if not way['bridge'] and way['layer'] == 0 and len(axis) >= 2]
        roads = [poly for poly in roads if poly.intersects(radius)]
        plan = dict(path=str(args.native_plan), sha256=hashlib.sha256(data).hexdigest(),
                    scope='Native pre-height-planning axes; footprint only, not a solved 3D plan.')
    if not buildings or not roads:
        raise ValueError(f'{place}: missing building/corridor input')
    return np.asarray(buildings, dtype=object), np.asarray(roads, dtype=object), dict(
        place=place, origin=[lat, lon], source_tiles=receipts, native_plan=plan,
        elevated_source_features_excluded=elevated, invalid_source_polygons_excluded=invalid)


def indexed_cut(buildings, roads):
    tree = shapely.STRtree(roads)
    pairs = tree.query(buildings, predicate='intersects')
    owners = np.unique(pairs[0])
    order = np.argsort(pairs[0], kind='stable')
    groups = pairs[:, order]
    ends = np.searchsorted(groups[0], owners, side='right')
    starts = np.r_[0, ends[:-1]]
    masks = np.asarray([shapely.union_all(roads[groups[1, a:b]])
                        for a, b in zip(starts, ends)], dtype=object)
    result = buildings.copy()
    result[owners] = shapely.difference(buildings[owners], masks)
    return result, dict(spatial_pairs=pairs.shape[1], candidates=len(owners),
                       possible_pairs=len(buildings) * len(roads))


def measure(call, repeats):
    durations, result = [], None
    for _ in range(repeats):
        begin = time.perf_counter()
        result = call()
        durations.append((time.perf_counter() - begin) * 1000)
    return result, dict(p50_ms=statistics.median(durations), maximum_ms=max(durations),
                       repeats=repeats)


def choose(buildings, cut, threshold, minimum_area, minimum_width):
    original = shapely.area(buildings)
    remaining = shapely.area(cut)
    affected = np.maximum(0, original - remaining) / original
    changed = affected > 1e-9
    result = cut.copy()
    discard = changed & ((affected >= threshold) | (remaining < minimum_area))
    result[discard] = Polygon()
    candidates = np.flatnonzero(changed & ~discard)
    pieces, owners = shapely.get_parts(cut[candidates], return_index=True)
    usable = shapely.area(pieces) >= minimum_area
    usable[usable] &= ~shapely.is_empty(shapely.buffer(pieces[usable], -minimum_width * .5))
    groups = np.full(len(candidates), Polygon(), dtype=object)
    if np.any(usable):
        shapely.multipolygons(pieces[usable], indices=owners[usable], out=groups)
    result[candidates] = groups
    discard[candidates] = shapely.is_empty(groups)
    return result, dict(cut=int(np.count_nonzero(changed & ~discard)),
                        discarded=int(np.count_nonzero(discard)),
                        affected_fraction_limit=threshold,
                        removed_area_fraction=1 - float(shapely.area(result).sum() / original.sum()))


def plot_geometry(ax, geometries, colour):
    from matplotlib.path import Path as PlotPath
    from matplotlib.patches import PathPatch
    for geometry in geometries:
        if geometry.is_empty:
            continue
        if geometry.geom_type != 'Polygon':
            plot_geometry(ax, geometry.geoms, colour)
            continue
        rings = [orient(geometry).exterior, *orient(geometry).interiors]
        points, codes = [], []
        for ring in rings:
            xy = list(ring.coords)
            points.extend(xy)
            codes.extend([PlotPath.MOVETO, *([PlotPath.LINETO] * (len(xy) - 2)), PlotPath.CLOSEPOLY])
        ax.add_patch(PathPatch(PlotPath(points, codes), facecolor=colour, edgecolor='#474a48',
                              linewidth=.15))


def render(args, name, buildings, roads, cut, hybrid, report):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(1, 3, figsize=(15, 5), constrained_layout=True)
    for ax, shapes, title in zip(axes, (buildings, cut, hybrid), ('Source', 'Cut', 'Cut / discard')):
        ax.set_facecolor('#dde9d0')
        plot_geometry(ax, roads, '#8e9796')
        plot_geometry(ax, shapes, '#cfbfa8')
        ax.set(xlim=(-args.view, args.view), ylim=(-args.view, args.view),
               aspect='equal', title=title, xlabel='East [m]', ylabel='North [m]')
    fig.suptitle(f"{name}: {len(buildings)} source footprints; "
                 f"{report['hybrid']['cut']} cut, {report['hybrid']['discarded']} discarded\n"
                 'Ground-level 2D experiment; no final height/profile proof')
    path = args.output / '2d' / f'{name}-building-clearance.png'
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=120)
    plt.close(fig)
    return str(path)


def verify():
    building = box(0, 0, 10, 10)
    touching = box(10, 0, 20, 10)
    narrow = box(9, 0, 20, 10)
    whole = box(-1, -1, 11, 11)
    original = np.asarray([building] * 3, dtype=object)
    cut = shapely.difference(original, [touching, narrow, whole])
    result, report = choose(original, cut, .25, 25, 3)
    assert result[0].equals(building) and result[1].area == 90 and result[2].is_empty
    assert report['cut'] == report['discarded'] == 1
    courtyard = Polygon([(0, 0), (20, 0), (20, 20), (0, 20)],
                        [[(5, 5), (5, 15), (15, 15), (15, 5)]])
    assert len(courtyard.difference(box(18, -1, 21, 21)).interiors) == 1
    pieces = shapely.union_all([building, box(20, 0, 21, 10)])
    result, _ = choose(np.asarray([pieces], dtype=object), np.asarray([
        pieces.difference(box(20, 9, 21, 11))], dtype=object), .25, 25, 3)
    assert result[0].equals(building)
    result, _ = indexed_cut(original, np.asarray([touching, narrow], dtype=object))
    assert all(p.equals(building.difference(narrow)) for p in result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--endpoint', required=True)
    parser.add_argument('--places', type=Path, default=Path('src/assets/places'))
    parser.add_argument('--native-plan', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--radius', type=float, default=800)
    parser.add_argument('--view', type=float, default=300)
    parser.add_argument('--side-room', type=float, default=2)
    parser.add_argument('--discard-fraction', type=float, default=.25)
    parser.add_argument('--minimum-area', type=float, default=25)
    parser.add_argument('--minimum-width', type=float, default=3)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('names', nargs='+')
    args = parser.parse_args()
    if (not all(math.isfinite(v) and v > 0 for v in (
            args.radius, args.view, args.minimum_area, args.minimum_width))
            or not 0 < args.discard_fraction <= 1 or args.side_room < 0
            or not math.isfinite(args.side_room) or args.repeats < 1):
        parser.error('finite positive dimensions/repeats and a fraction in (0, 1] required')
    verify()
    templates = json.loads(Path('src/assets/world/vegetation.json').read_text())['templates']
    rules = [r for t in templates for r in t.get('osm', [])
             if r.get('layer') == 'streets' and r.get('widthM', 0) > 0]
    widths = {r['kind']: r['widthM'] for r in sorted(rules, key=lambda r: r.get('rank', 0))}
    args.output.mkdir(parents=True, exist_ok=True)
    reports = []
    for name in args.names:
        begin = time.perf_counter()
        buildings, roads, report = load_scene(args, name, widths)
        report['decode_ms'] = (time.perf_counter() - begin) * 1000
        (cut, counts), indexed_time = measure(lambda: indexed_cut(buildings, roads), args.repeats)
        global_cut, global_time = measure(lambda: shapely.difference(
            buildings, shapely.union_all(roads)), args.repeats)
        error = float(shapely.area(shapely.symmetric_difference(cut, global_cut)).max())
        if error > 1e-6 or not np.all(shapely.is_valid(cut)):
            raise ValueError(f'{name}: indexed/global geometry mismatch or invalid output')
        (hybrid, choice), choose_time = measure(lambda: choose(buildings, cut, args.discard_fraction,
            args.minimum_area, args.minimum_width), args.repeats)
        original, remaining = shapely.area(buildings), shapely.area(cut)
        report.update(buildings=len(buildings), corridor_parts=len(roads), **counts,
                      indexed_cut=indexed_time, global_cut=global_time, hybrid_selection=choose_time,
                      maximum_symmetric_difference_m2=error, hybrid=choice,
                      discard_any_count=int(np.count_nonzero(original - remaining > 1e-7)),
                      side_room_m=args.side_room, minimum_area_m2=args.minimum_area,
                      minimum_width_m=args.minimum_width, radius_m=args.radius)
        report['image'] = render(args, name, buildings, roads, cut, hybrid, report)
        reports.append(report)
        (args.output / 'building-clearance.json').write_text(json.dumps(reports, indent=2) + '\n')
        print(json.dumps({k: report[k] for k in ('place', 'buildings', 'candidates',
                         'indexed_cut', 'global_cut', 'hybrid', 'discard_any_count')}), flush=True)


if __name__ == '__main__':
    main()
