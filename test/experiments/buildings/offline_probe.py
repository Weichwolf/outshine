"""Read an enriched plan in a fresh process with source acquisition and planning forbidden."""

import hashlib
import json
from pathlib import Path
import sys
from unittest.mock import patch

import numpy as np

from codec import decode
from demand import View, select
from facade_geometry import mesh


def probe(data):
    product = decode(data)
    signatures = []
    for eye in ((50., 50., 25.), (-50., -50., 25.), (3000., -3000., 1000.)):
        demand = select(product, View(np.array(eye), np.array([0., 0., 7.])))
        geometry = mesh(product, demand)
        digest = hashlib.sha256()
        for key in ('vertices', 'faces', 'uv', 'materials'):
            digest.update(geometry[key].tobytes())
        signatures.append(dict(sha256=digest.hexdigest(), triangles=len(geometry['faces']),
                               openings=geometry['detail_openings']))
    return signatures


if __name__ == '__main__':
    with patch('samples.read_site', side_effect=AssertionError('offline hit acquired source')), \
         patch('facade_plan.plan', side_effect=AssertionError('offline hit rebuilt plan')), \
         patch('shapely.constrained_delaunay_triangles', side_effect=AssertionError('offline hit rebuilt shell')):
        print(json.dumps(probe(Path(sys.argv[1]).read_bytes())))
