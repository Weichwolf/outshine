"""Experiment-only serialization of enriched camera-independent plans; not a cache manager."""

from dataclasses import asdict
import json

import numpy as np
import shapely

from facade_plan import Repeat
from massing import footprint_of
from shell import FIELDS, validate
from recipes import FAMILIES

SCHEMA = 4


def encode(product):
    body = {k: product[k] for k in ('id', 'top_m', 'bottom_m', 'style', 'seed', 'source_properties', 'provenance',
                                  'massing', 'surfaces', 'features', 'feature_bounds', 'opening_patterns', 'context', 'recipe')}
    body['layers'] = [dict(bottom_m=layer['bottom_m'], top_m=layer['top_m'], floors=layer['floors'],
                           polygons=[[ring.tolist() for ring in rings] for rings in layer['polygons']])
                      for layer in product['layers']]
    body['wall_linear'] = product['wall_linear'].tolist()
    body['facades'] = [dict(normal=f['normal'].tolist(), repeat=asdict(f['repeat']),
                           frontage_sources=f.get('frontage_sources', [])) for f in product['facades']]
    body['shell'] = {k: product['shell'][k].tolist() for k in FIELDS}
    body['decoration'] = {k: product['decoration'][k].tolist() for k in FIELDS}
    return json.dumps(dict(schema=SCHEMA, product=body), sort_keys=True, separators=(',', ':'), allow_nan=False).encode()


def decode_layers(product, heights):
    records = product['layers']
    if not isinstance(records, list) or not 1 <= len(records) <= 3:
        raise ValueError('invalid encoded layer count')
    layers, low = [], heights[0]
    for layer in records:
        bottom, top, floors = layer['bottom_m'], layer['top_m'], layer['floors']
        if (not np.isfinite([bottom, top]).all() or abs(bottom-low) > 1e-10 or top <= bottom
                or type(floors) is not int or floors < 1):
            raise ValueError('invalid encoded floor band')
        polygons = [[np.asarray(ring, dtype=np.float64) for ring in rings] for rings in layer['polygons']]
        if not polygons or any(not rings for rings in polygons) or any(
                r.ndim != 2 or r.shape[1] != 2 or len(r) < 3 or not np.isfinite(r).all()
                for rings in polygons for r in rings):
            raise ValueError('invalid encoded layer rings')
        footprint = footprint_of(polygons)
        if (not footprint.is_valid or footprint.area <= 0 or (layers and
                (not layers[-1]['footprint'].covers(footprint)
                 or layers[-1]['footprint'].boundary.distance(footprint.boundary) <= 1e-6))):
            raise ValueError('invalid encoded nested footprint')
        layers.append(dict(bottom_m=bottom, top_m=top, floors=floors, polygons=polygons, footprint=footprint))
        low = top
    if abs(low-heights[1]) > 1e-10 or len(layers[0]['polygons']) != 1:
        raise ValueError('encoded bands disagree with source interval')
    decision = product['massing']
    if (sum(layer['floors'] for layer in layers) != decision['floors']
            or len(decision['offsets_m']) != len(layers)
            or any(not np.isclose((layer['top_m']-layer['bottom_m'])/layer['floors'],
                                  decision['floor_height_m'], atol=1e-10, rtol=1e-12) for layer in layers)):
        raise ValueError('encoded floor rhythm disagrees with massing plan')
    return layers


def decode_record(data):
    record = json.loads(data)
    if not isinstance(record, dict) or record.get('schema') not in (3, SCHEMA):
        raise ValueError('unsupported enriched-plan schema')
    product = record.get('product')
    required = {'id', 'top_m', 'bottom_m', 'style', 'seed', 'source_properties', 'provenance', 'layers',
                'wall_linear', 'facades', 'shell', 'massing', 'surfaces', 'features', 'decoration',
                'feature_bounds', 'opening_patterns'}
    if not isinstance(product, dict) or required-set(product):
        raise ValueError('incomplete enriched plan')
    if record['schema'] == SCHEMA and not {'context', 'recipe'} <= product.keys():
        raise ValueError('incomplete semantic recipe')
    if record['schema'] == 3:
        product['context'] = dict(version=1, evidence=[])
        product['recipe'] = dict(version=1, family='unknown', origin='legacy-schema-3', openings='unavailable')
    if (not isinstance(product['context'], dict) or product['context'].get('version') != 1
            or not isinstance(product['context'].get('evidence'), list)
            or not isinstance(product['recipe'], dict) or product['recipe'].get('version') != 1):
        raise ValueError('invalid encoded semantic recipe')
    if (product['recipe'].get('family') not in FAMILIES or product['recipe'].get('openings') not in ('unavailable', 'procedural-context')
            or not all(isinstance(row, dict) and isinstance(row.get('id'), str) and isinstance(row.get('layer'), str)
                       and isinstance(row.get('properties'), dict) for row in product['context']['evidence'])):
        raise ValueError('invalid encoded contextual evidence')
    heights = np.asarray([product['bottom_m'], product['top_m']], dtype=float)
    if not np.isfinite(heights).all() or heights[1] <= heights[0]:
        raise ValueError('invalid encoded height interval')
    if (product['style'] not in ('osm', 'bauhaus', 'midcentury', 'artdeco') or type(product['seed']) is not int
            or not isinstance(product['source_properties'], dict) or not isinstance(product['provenance'], dict)):
        raise ValueError('invalid encoded recipe')
    product['layers'] = decode_layers(product, heights)
    product['rings'] = product['layers'][0]['polygons'][0]
    product['wall_linear'] = np.asarray(product['wall_linear'], dtype=float)
    if (not product['rings'] or any(r.ndim != 2 or r.shape[1] != 2 or len(r) < 3
                                  or not np.isfinite(r).all() for r in product['rings'])
            or product['wall_linear'].shape != (3,) or not np.isfinite(product['wall_linear']).all()
            or np.any(product['wall_linear'] < 0) or np.any(product['wall_linear'] > 1)):
        raise ValueError('invalid encoded shape or colour')
    records = product['facades']
    if sum(len(r) for layer in product['layers'] for rings in layer['polygons'] for r in rings) != len(records):
        raise ValueError('incomplete facade plan')
    facades = []
    for index, layer in enumerate(product['layers']):
        for component in layer['polygons']:
            for ring in component:
                for a, b in zip(ring, np.roll(ring, -1, axis=0)):
                    record = records[len(facades)]
                    normal, repeat = np.asarray(record['normal'], dtype=float), Repeat(**record['repeat'])
                    length = np.linalg.norm(b-a)
                    expected = np.r_[(b-a)[1], -(b-a)[0], 0.]/max(length, 1e-12)
                    if (length <= 1e-8 or normal.shape != (3,) or not np.isfinite(normal).all()
                            or not np.allclose(normal, expected, atol=1e-10, rtol=0)
                            or not np.isclose(repeat.width_m, length, atol=1e-10, rtol=1e-12)
                            or not np.isclose(repeat.height_m, layer['top_m']-layer['bottom_m'], atol=1e-10, rtol=1e-12)
                            or repeat.rows != layer['floors']):
                        raise ValueError('encoded facade disagrees with shape')
                    sources = record.get('frontage_sources', [])
                    if not isinstance(sources, list) or not all(isinstance(s, str) for s in sources):
                        raise ValueError('invalid encoded frontage references')
                    facades.append(dict(a=a, b=b, normal=normal, repeat=repeat, layer=index, frontage_sources=sources,
                                        bottom_m=layer['bottom_m'], top_m=layer['top_m']))
    product['facades'] = facades
    product['polygon'] = shapely.Polygon(product['rings'][0], product['rings'][1:])
    surfaces = product['surfaces']
    if not isinstance(surfaces, list) or len(surfaces) != len(product['layers'])+1:
        raise ValueError('incomplete horizontal surfaces')
    for index, surface in enumerate(surfaces):
        upward = index < len(product['layers'])
        height = product['layers'][index]['top_m'] if upward else heights[0]
        bounds = np.asarray(surface['bounds'], dtype=float)
        if (surface['part'] != -index-1 or surface['upward'] is not upward or surface['height_m'] != height
                or bounds.shape != (4,) or not np.isfinite(bounds).all() or np.any(bounds[2:] <= bounds[:2])):
            raise ValueError('invalid encoded horizontal surface')
    patterns = sorted({(f['repeat'].opening_width, f['repeat'].opening_height) for f in facades if f['repeat'].count})
    if [list(pattern) for pattern in patterns] != product['opening_patterns']:
        raise ValueError('encoded material table disagrees with opening plans')
    product['opening_patterns'] = patterns
    validate(product['shell'], len(facades), surfaces, len(patterns))
    validate_features(product)
    if product['style'] == 'osm' and (product['massing']['mode'] != 'plain' or len(product['layers']) != 1
                                     or product['features'] or (product['recipe'].get('openings') == 'unavailable'
                                                               and any(f['repeat'].count for f in product['facades']))):
        raise ValueError('encoded OSM body contains unsupported procedural additions')
    return product


def decode(data):
    try:
        return decode_record(data)
    except (KeyError, TypeError, IndexError) as error:
        raise ValueError('malformed enriched plan') from error


def validate_features(product):
    cached = product['decoration']
    for name, dtype in zip(FIELDS, (np.float64, np.int32, np.float32, np.uint8, np.int32)):
        cached[name] = np.asarray(cached[name], dtype=dtype)
    vertices, faces, uv = (cached[k].reshape(-1, n) for k, n in (('vertices', 3), ('faces', 3), ('uv', 2)))
    cached.update(vertices=vertices, faces=faces, uv=uv)
    count = len(product['features'])
    if (not np.isfinite(vertices).all() or not np.isfinite(uv).all() or len(uv) != len(vertices)
            or cached['materials'].shape != (len(faces),) or np.any(cached['materials'] > 5)
            or cached['parts'].shape != (len(faces),) or len(product['feature_bounds']) != count
            or (len(faces) and (faces.min() < 0 or faces.max() >= len(vertices)
                               or cached['parts'].min() < 0 or cached['parts'].max() >= count))):
        raise ValueError('invalid prepared feature buffers')
    for index, bounds in enumerate(product['feature_bounds']):
        selected = faces[cached['parts'] == index]
        if not len(selected):
            raise ValueError('incomplete prepared feature coverage')
        points = vertices[selected].reshape(-1, 3)
        if np.asarray(bounds).shape != (6,) or not np.allclose(bounds, np.r_[points.min(0), points.max(0)], atol=1e-10, rtol=0):
            raise ValueError('encoded feature bounds disagree with prepared geometry')
