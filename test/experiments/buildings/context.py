"""Deterministic spatial context joins, performed once before building asset preparation."""

from collections import Counter
import json
import time

import numpy as np
import shapely

from context_catalog import AREA_LAYERS, POINT_LAYERS, NEAR_LAYERS, ACCESS_CLASSES, catalog, poi_role

NEAR_M = 12.
SITE_COVERAGE = .8


def records(layers, project, source_sha256):
    result, rejected = {}, []
    for name in sorted(AREA_LAYERS | POINT_LAYERS | NEAR_LAYERS):
        layer = layers.get(name, {})
        rows = []
        for ordinal, feature in enumerate(layer.get('features', [])):
            key = f'{name}:{feature.get("id", "unknown")}:{ordinal}'
            try:
                geometry = shapely.from_geojson(json.dumps(feature['geometry']))
                geometry = shapely.transform(geometry, lambda xy: project(xy, layer.get('extent', 4096)))
            except (ValueError, shapely.errors.GEOSException) as error:
                rejected.append(dict(id=key, layer=name, reason='invalid context geometry', detail=str(error)))
                continue
            parts = list(shapely.get_parts(geometry))
            for part, shape in enumerate(parts):
                if shape.is_empty or not shape.is_valid or not np.isfinite(shape.bounds).all():
                    rejected.append(dict(id=key+f':{part}', layer=name, reason='invalid context geometry'))
                    continue
                if name in POINT_LAYERS and shape.geom_type != 'Point':
                    rejected.append(dict(id=key, layer=name, reason='point layer contains non-point geometry'))
                    continue
                rows.append(dict(id=key+f':{part}', layer=name, geometry=shape,
                                 properties=feature['properties'].copy(), source_sha256=source_sha256))
        result[name] = rows
    return result, rejected


def witness(record, relation, **scope):
    return {k: record[k] for k in ('id', 'layer', 'properties', 'source_sha256')} | dict(relation=relation, **scope)


def joins(samples, rows, indexed):
    if not rows:
        return []
    polygons = [sample['polygon'] for sample in samples]
    pairs = shapely.STRtree(polygons).query([r['geometry'] for r in rows], predicate='intersects') if indexed else None
    candidates = [[] for _ in rows]
    if indexed:
        for source, host in pairs.T:
            candidates[source].append(int(host))
    else:
        for source, record in enumerate(rows):
            candidates[source] = [i for i, p in enumerate(polygons) if p.intersects(record['geometry'])]
    return [sorted(values) for values in candidates]


def point_hosts(samples, rows, contexts, indexed):
    unused = []
    for record, candidates in zip(rows, joins(samples, rows, indexed)):
        point, properties = record['geometry'], record['properties']
        hosts = [i for i in candidates if samples[i]['polygon'].covers(point)]
        if properties.get('level') in (0, '0'):
            hosts = [i for i in hosts if float(samples[i]['properties'].get('render_min_height', 0)) <= 0]
        if len(hosts) != 1:
            unused.append(dict(id=record['id'], layer=record['layer'], reason='ambiguous host' if hosts else 'no containing host',
                               candidates=[samples[i]['id'] for i in hosts]))
            continue
        role = poi_role(properties) if record['layer'] == 'poi' else 'address'
        contexts[hosts[0]]['evidence'].append(witness(record, 'unique-point-host', role=role,
                          position_xy=list(list(point.coords)[0]), scope='address' if role == 'address' else 'partial-use',
                          level=properties.get('level'), indoor=properties.get('indoor'), vertical_layer=properties.get('layer')))
    return unused


def area_hosts(samples, rows, contexts, indexed):
    associated = set()
    for record, candidates in zip(rows, joins(samples, rows, indexed)):
        geometry = record['geometry']
        if geometry.geom_type not in ('Polygon', 'MultiPolygon'):
            continue
        for host in candidates:
            polygon = samples[host]['polygon']
            area = polygon.intersection(geometry).area
            if area <= 0:
                continue
            associated.add(record['id'])
            contexts[host]['evidence'].append(witness(record, 'area-overlap', coverage=area/polygon.area,
                                           context_area_m2=geometry.area, scope='site-context'))
    return associated


def nearby_hosts(samples, rows, contexts, indexed):
    geometries = [r['geometry'] for r in rows]
    tree = shapely.STRtree(geometries) if indexed else None
    associated = set()
    for host, sample in enumerate(samples):
        boundary = sample['polygon'].exterior
        candidates = tree.query(boundary, predicate='dwithin', distance=NEAR_M) if indexed else range(len(rows))
        for index in sorted(candidates):
            record, geometry = rows[index], geometries[index]
            distance = boundary.distance(geometry)
            if distance > NEAR_M:
                continue
            properties = record['properties']
            usable = (record['layer'] == 'transportation' and properties.get('class') in ACCESS_CLASSES
                      and properties.get('brunnel') not in ('bridge', 'tunnel')
                      and properties.get('layer', 0) in (0, '0') and not properties.get('indoor')
                      and properties.get('level', 0) in (0, '0') and properties.get('access') not in ('no', 'private'))
            line = shapely.shortest_line(boundary, geometry)
            point = list(line.coords)[-1] if line is not None else list(geometry.representative_point().coords)[0]
            contexts[host]['evidence'].append(witness(record, 'nearby', distance_m=distance,
                                    position_xy=list(point), scope='frontage-candidate' if usable else 'surroundings',
                                    usable_frontage=usable))
            associated.add(record['id'])
    return associated


def site_use(context, records_by_id, polygon):
    groups = {}
    for row in context['evidence']:
        if row['layer'] == 'landuse' and row['relation'] == 'area-overlap':
            groups.setdefault(str(row['properties'].get('class', 'unknown')), []).append(row)
    candidates = []
    for value, rows in groups.items():
        geometry = shapely.union_all([records_by_id[r['id']]['geometry'] for r in rows])
        if polygon.intersection(geometry).area/polygon.area >= SITE_COVERAGE:
            candidates.append(dict(value=value, geometry=geometry, ids=[r['id'] for r in rows]))
    minimal = [r for r in candidates if not any(r['value'] != s['value'] and r['geometry'].covers(s['geometry'])
               and not s['geometry'].covers(r['geometry']) for s in candidates)]
    classes = sorted(r['value'] for r in minimal)
    return dict(value=classes[0] if len(classes) == 1 else 'mixed' if classes else 'unknown',
                origin='landuse-context' if classes else 'unavailable', evidence_ids=sorted(i for r in minimal for i in r['ids']),
                reason='dominant coverage; nested site beats its surroundings' if len(classes) == 1
                       else 'conflicting overlapping site classes' if classes else 'no dominant landuse')


def attach(samples, layers, project, source_sha256, indexed=True):
    began = time.perf_counter()
    inputs, rejected = records(layers, project, source_sha256)
    contexts = [dict(version=1, source_sha256=source_sha256, evidence=[]) for _ in samples]
    unused, associated = [], set()
    by_id = {r['id']:r for rows in inputs.values() for r in rows}
    for name, rows in inputs.items():
        if name in POINT_LAYERS:
            excluded = point_hosts(samples, rows, contexts, indexed)
            unused.extend(excluded)
            associated.update(set(r['id'] for r in rows)-{r['id'] for r in excluded})
        if name in AREA_LAYERS:
            associated.update(area_hosts(samples, rows, contexts, indexed))
        if name in NEAR_LAYERS:
            associated.update(nearby_hosts(samples, rows, contexts, indexed))
    for key, record in by_id.items():
        if key not in associated and record['layer'] not in POINT_LAYERS:
            unused.append(dict(id=key, layer=record['layer'], reason='no building overlap or nearby boundary'))
    for sample, context in zip(samples, contexts):
        context['evidence'].sort(key=lambda r:(r['layer'], r['id'], r['relation']))
        context['site_use'] = site_use(context, by_id, sample['polygon'])
        sample['context'] = context
    return dict(method='STRtree with exact predicates' if indexed else 'exhaustive exact predicates',
                preparation_ms=(time.perf_counter()-began)*1000, near_m=NEAR_M, site_coverage=SITE_COVERAGE,
                layers=catalog(layers), rejected=rejected, unused=unused,
                associated_context_features=len(associated), projected_context_features=len(by_id),
                site_uses=dict(Counter(c['site_use']['value'] for c in contexts)),
                raw_sources_retained=True, regional_metadata='original MVT and catalog; not duplicated per building')
