"""Experiment-only serialization of enriched camera-independent plans; not a cache manager."""

from dataclasses import asdict
import json

import numpy as np
import shapely

from facade_plan import Repeat
from shell import FIELDS, validate

SCHEMA = 2


def encode(product):
    body = {k: product[k] for k in ('id', 'top_m', 'bottom_m', 'style', 'seed', 'source_properties', 'provenance')}
    body['rings'] = [r.tolist() for r in product['rings']]
    body['wall_linear'] = product['wall_linear'].tolist()
    body['facades'] = [dict(normal=f['normal'].tolist(), repeat=asdict(f['repeat'])) for f in product['facades']]
    body['shell'] = {k: product['shell'][k].tolist() for k in FIELDS}
    return json.dumps(dict(schema=SCHEMA, product=body), sort_keys=True, separators=(',', ':'), allow_nan=False).encode()


def decode(data):
    record = json.loads(data)
    if not isinstance(record, dict) or record.get('schema') != SCHEMA:
        raise ValueError('unsupported enriched-plan schema')
    product = record.get('product')
    required = {'id', 'top_m', 'bottom_m', 'style', 'seed', 'source_properties', 'provenance', 'rings', 'wall_linear', 'facades', 'shell'}
    if not isinstance(product, dict) or required-set(product):
        raise ValueError('incomplete enriched plan')
    heights = np.asarray([product['bottom_m'], product['top_m']], dtype=float)
    if not np.isfinite(heights).all() or heights[1] <= heights[0]:
        raise ValueError('invalid encoded height interval')
    if (product['style'] not in ('bauhaus', 'artdeco') or type(product['seed']) is not int
            or not isinstance(product['source_properties'], dict) or not isinstance(product['provenance'], dict)):
        raise ValueError('invalid encoded recipe')
    product['rings'] = [np.asarray(r, dtype=np.float64) for r in product['rings']]
    product['wall_linear'] = np.asarray(product['wall_linear'], dtype=float)
    if (not product['rings'] or any(r.ndim != 2 or r.shape[1] != 2 or len(r) < 3
                                  or not np.isfinite(r).all() for r in product['rings'])
            or product['wall_linear'].shape != (3,) or not np.isfinite(product['wall_linear']).all()
            or np.any(product['wall_linear'] < 0) or np.any(product['wall_linear'] > 1)):
        raise ValueError('invalid encoded shape or colour')
    records = product['facades']
    if sum(map(len, product['rings'])) != len(records):
        raise ValueError('incomplete facade plan')
    facades = []
    for ring in product['rings']:
        for a, b in zip(ring, np.roll(ring, -1, axis=0)):
            record = records[len(facades)]
            normal, repeat = np.asarray(record['normal'], dtype=float), Repeat(**record['repeat'])
            length = np.linalg.norm(b-a)
            expected = np.r_[(b-a)[1], -(b-a)[0], 0.]/max(length, 1e-12)
            if (length <= 1e-8 or normal.shape != (3,) or not np.isfinite(normal).all()
                    or not np.allclose(normal, expected, atol=1e-10, rtol=0)
                    or not np.isclose(repeat.width_m, length, atol=1e-10, rtol=1e-12)
                    or not np.isclose(repeat.height_m, heights[1]-heights[0], atol=1e-10, rtol=1e-12)):
                raise ValueError('encoded facade disagrees with shape')
            facades.append(dict(a=a, b=b, normal=normal, repeat=repeat))
    product['facades'] = facades
    product['polygon'] = shapely.Polygon(product['rings'][0], product['rings'][1:])
    if not product['polygon'].is_valid:
        raise ValueError('invalid encoded footprint')
    validate(product['shell'], len(facades))
    return product
