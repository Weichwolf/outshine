from dataclasses import replace
import argparse
import hashlib
import json
from pathlib import Path

from shapely.geometry import Polygon, box

from canonical_polygons import Observation, assemble, components, load_exports, resolve, shape_token

ROOT = Path(__file__).parent
def fixture_paths():
    manifest = json.loads((ROOT / 'inputs' / 'manifest.json').read_text())
    paths = []
    for record in manifest['files']:
        path = ROOT / 'inputs' / record['file']
        if hashlib.sha256(path.read_bytes()).hexdigest() != record['sha256']:
            raise ValueError('fixed original input changed: ' + record['file'])
        paths.append(path)
    if not paths:
        raise ValueError('no fixed original inputs')
    return paths


NAMESPACE = ('declared-negative-control', 'epoch-1', 'buildings', 14)


def observation(polygon, tile=(14, 0, 0), window=(-64, -64, 4160, 4160), ordinal=0):
    return Observation(NAMESPACE, tile, 'declared-input', 79947880, ordinal,
                       'same-material-and-height', polygon, window)


def semantic(result):
    return {k: v for k, v in result.items() if k != 'proof'}


def run_checks():
    checks = []
    def check(name, condition):
        checks.append({'name': name, 'passed': bool(condition)})
        assert condition, name
    paths = fixture_paths()
    originals, residual = load_exports(paths)
    point = (8744 * 4096 + 587, 5705 * 4096 + 42)
    pair = [o for o in originals if point in list(o.polygon.exterior.coords)]
    check('original ring pair remains two observations', len(pair) == 2)
    groups = components(pair)
    check('original overlap forms one spatial component', len(groups) == 1)
    result = resolve(pair)
    check('original component has one canonical whole footprint', result['status'] == 'ready' and result['area'] == 2050)
    check('alternate execution order agrees on original', semantic(resolve(pair, 'boundary-first')) == semantic(result))
    check('source order does not change canonical product', semantic(resolve(pair[::-1])) == semantic(result))
    unrelated = [observation(box(100, 100, 120, 120), ordinal=0), observation(box(500, 500, 520, 520), ordinal=1)]
    check('one merged container keeps two disjoint houses', len(components(unrelated)) == 2)
    reidentified = [replace(o, container=10 + i, ordinal=200 + i) for i, o in enumerate(pair)]
    check('container IDs and ring ordinals are not building identity', len(components(reidentified)) == 1 and resolve(reidentified)['geometry_digest'] == result['geometry_digest'])
    changed = [replace(o, input_digest='changed-payload') for o in pair]
    check('equivalent geometry keeps semantic digest', resolve(changed)['geometry_digest'] == result['geometry_digest'])
    check('changed dependencies get a different proof digest', resolve(changed)['dependency_digest'] != result['dependency_digest'])
    variants = [unrelated[0], replace(unrelated[0], namespace=('another-dataset',), tile=(14, 0, 1))]
    check('different dataset does not coalesce', len(components(variants)) == 2)
    mixed_rejected = False
    try:
        assemble(variants, NAMESPACE)
    except ValueError:
        mixed_rejected = True
    check('mixed snapshots require upstream source admission', mixed_rejected)
    full = max(pair, key=lambda o: o.polygon.area)
    coordinates = list(full.polygon.exterior.coords)[:-1]
    rotated = replace(full, polygon=Polygon(coordinates[2:] + coordinates[:2]))
    reversed_ring = replace(full, polygon=Polygon(coordinates[::-1]))
    check('cyclic ring start does not change canonical geometry', resolve([rotated])['geometry_digest'] == result['geometry_digest'])
    check('ring winding does not change canonical geometry', resolve([reversed_ring])['geometry_digest'] == result['geometry_digest'])
    conflicting = [unrelated[0], replace(unrelated[0], tile=(14, 0, 1), attributes='different-material')]
    check('conflicting attributes are retained as unresolved input', resolve(conflicting)['status'] == 'conflicting-attributes')
    overlapping_whole = [unrelated[0], observation(box(110, 100, 130, 120), tile=(14, 0, 1))]
    check('different whole polygons cannot be silently unioned', resolve(overlapping_whole)['status'] == 'conflicting-whole-polygons')
    rectangle = box(100, -100, 300, 100)
    windows = [(0, -4096, 4096, 64), (0, -64, 4096, 4096)]
    crossing = [observation(rectangle.intersection(box(*w)), tile=(14, 0, i), window=w) for i, w in enumerate(windows)]
    crossed = resolve(crossing)
    check('crossing polygon reconstructs without a whole source copy', crossed['status'] == 'ready' and crossed['shape'] == shape_token(rectangle))
    with_hole = Polygon([(100, -100), (300, -100), (300, 100), (100, 100)],
                        [[(150, -20), (150, 20), (250, 20), (250, -20)]])
    holed = [observation(with_hole.intersection(box(*w)), tile=(14, 0, i), window=w) for i, w in enumerate(windows)]
    combined = resolve(holed)
    check('courtyard survives cross-tile reconstruction', combined['status'] == 'ready' and combined['shape'] == shape_token(with_hole))
    for i, item in enumerate(crossing):
        check('missing neighbor is unresolved, not a smaller building ' + str(i), resolve([item])['status'] != 'ready')
    # True distinct outlines in one tile remain separate even when the container groups them.
    same_tile = [observation(box(100, 100, 200, 200)), observation(box(150, 150, 250, 250), ordinal=1)]
    check('distinct co-observed polygons are not fused by overlap alone', len(components(same_tile)) == 2)
    return {'checks': checks, 'count': len(checks), 'original_observations': len(originals),
            'max_grid_inverse_residual': residual, 'original_result': result}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Check source-polygon identity and reconstruction controls.')
    parser.add_argument('--output', type=Path, help='Optional JSON report outside the fixed inputs.')
    args = parser.parse_args()
    result = run_checks()
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('CHECKS', result['count'], 'FAILURES 0')
