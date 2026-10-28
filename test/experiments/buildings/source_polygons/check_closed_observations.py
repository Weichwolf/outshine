"""Conditional cardinality control for complete post-merge polygon observations."""
import argparse
from collections import Counter
from dataclasses import replace
from fractions import Fraction as F
import hashlib
import json
from pathlib import Path

from shapely.geometry import Polygon

from canonical_polygons import load_exports, whole
from check_reference import fixture_paths
from clip_round_witness import clip, cross, rounded, simple, token


def convex_simple(ring):
    if not simple(ring):
        return False
    turns = [cross(ring[i - 2], ring[i - 1], ring[i]) for i in range(len(ring))]
    return all(turn >= 0 for turn in turns) or all(turn <= 0 for turn in turns)


def candidate(observation, ring=None):
    if observation.polygon.interiors:
        raise ValueError('this conditional model does not handle holes')
    points = list(observation.polygon.exterior.coords)[:-1] if ring is None else ring
    return observation.namespace, observation.attributes, tuple(points)


def observed_key(observation):
    polygon = observation.polygon
    return (observation.tile, observation.namespace, observation.attributes,
            token(list(polygon.exterior.coords)[:-1]),
            tuple(sorted(token(list(r.coords)[:-1]) for r in polygon.interiors)))


def modeled_counts(candidates, windows):
    result = Counter()
    for namespace, attributes, ring in candidates:
        if not convex_simple(ring):
            return None
        for tile, window in windows.items():
            shape = clip(ring, window)
            if shape is None:
                return None
            if not shape:
                continue
            shape = rounded(shape)
            if not simple(shape):
                return None
            result[(tile, namespace, attributes, token(shape), ())] += 1
    return result


def check_model(candidates, observations, windows, complete_tiles):
    if complete_tiles != set(windows):
        return 'unclosed'
    expected = modeled_counts(candidates, windows)
    if not expected:
        return 'unsupported'
    catalog = Counter(observed_key(o) for o in observations)
    return 'supported' if not (expected - catalog) else 'contradicted'


def run_checks():
    paths = fixture_paths()
    original, _ = load_exports(paths)
    point = (8744 * 4096 + 587, 5705 * 4096 + 42)
    pair = [o for o in original if point in list(o.polygon.exterior.coords)]
    assert len(pair) == 2 and all(not o.polygon.interiors for o in pair)
    full = next(o for o in pair if whole(o))
    cut = next(o for o in pair if not whole(o))
    windows = {o.tile: o.window for o in pair}
    closed = set(windows)
    checks = []

    def check(name, condition):
        assert condition, name
        checks.append(name)

    def state(candidates, observations=original, complete=closed):
        return check_model(candidates, observations, windows, complete)

    ring = [(F(int(x)) + (F(1, 4) if int(x) % 4096 in (649, 641) else 0), F(int(y)))
            for x, y in list(full.polygon.exterior.coords)[:-1]]
    one = [candidate(full, ring)]
    two = one + [candidate(cut)]
    check('one shared rational whole predicts exactly the original two rings',
          modeled_counts(one, windows) == Counter(observed_key(o) for o in pair))
    check('whole is supported by the complete 2853-observation catalog', state(one) == 'supported')
    check('second cut geometry predicts four observations', sum(modeled_counts(two, windows).values()) == 4)
    check('two-geometry alternative exceeds complete catalog multiplicity', state(two) == 'contradicted')
    check('missing neighbor completeness remains unclosed', state(one, complete=closed - {cut.tile}) == 'unclosed')
    check('complete but missing full observation contradicts candidate', state(one, [cut]) == 'contradicted')
    check('two copies of each observation support two latent wholes', state(one + one, pair + pair) == 'supported')
    check('two original observations cannot support two coincident wholes', state(one + one, pair) == 'contradicted')
    check('attribute mismatch cannot consume original observations',
          state([(full.namespace, 'different-attributes', tuple(ring))]) == 'contradicted')
    check('revision mismatch cannot consume original observations',
          state([(full.namespace + ('changed-revision',), full.attributes, tuple(ring))]) == 'contradicted')
    check('container relabeling does not alter cardinality',
          state(one, [replace(o, container=None) for o in original]) == 'supported')
    check('observation and candidate order do not alter cardinality',
          modeled_counts(two, windows) == modeled_counts(two[::-1], windows)
          and state(one, original[::-1]) == 'supported')
    check('candidate outside supplied windows is unsupported',
          state([(full.namespace, full.attributes, tuple((x + 100000, y + 100000) for x, y in ring))]) == 'unsupported')
    interior = [o for o in original if whole(o) and not o.polygon.interiors
                and o.polygon.equals(o.polygon.convex_hull)
                and o.polygon.bounds[3] < max(w[1] for w in windows.values())]
    first = interior[0]
    second = next(o for o in interior[1:] if o.attributes == first.attributes and o.polygon.disjoint(first.polygon))
    distinct = [candidate(first), candidate(second)]
    check('two actual distinct complete same-attribute buildings remain supported', state(distinct) == 'supported')
    check('dropping either actual distinct observation is detected',
          state(distinct, [o for o in original if o is not second]) == 'contradicted')
    check('concave candidate is outside the declared convex model',
          state([(full.namespace, full.attributes, ((0, 0), (4, 0), (1, 1), (4, 4), (0, 4)))]) == 'unsupported')
    holed = replace(full, polygon=Polygon([(0, 0), (4, 0), (4, 4), (0, 4)],
                                         [[(1, 1), (1, 2), (2, 2), (2, 1)]]))
    try:
        candidate(holed)
    except ValueError:
        checks.append('holed candidate is explicitly refused')
    else:
        raise AssertionError('holed candidate was silently reduced to its exterior')
    predicted = modeled_counts(two, windows)
    catalog = Counter(observed_key(o) for o in original)
    return {
        'controls': checks, 'failures': 0, 'observations': len(original),
        'one_geometry_predicted': sum(modeled_counts(one, windows).values()),
        'two_geometries_predicted': sum(predicted.values()),
        'two_geometry_excess': [dict(tile=key[0], missing=missing, predicted=predicted[key],
                                    observed=catalog[key], ring=key[3])
                                for key, missing in (predicted - catalog).items()],
        'inputs': [dict(file=path.name, sha256=hashlib.sha256(path.read_bytes()).hexdigest()) for path in paths],
        'model': 'Complete supplied tile catalogs; convex simple hole-free post-merge latent polygons '
                 'emit their exact clip/round ring in each intersecting window, without additional '
                 'simplification, dropout, attribute change, merging or unmodeled repair.',
        'limits': ['Post-merge geometry counts are not original OSM object counts.',
                   'Historical provider clipping/rounding/simplification is not attested.',
                   'Supported means the proposal fits available counts, not uniqueness or a global reconstruction.',
                   'Unknown windows, general holes, disconnected clipping and native publication remain open.'],
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = run_checks()
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(text)
    print(text, end='')
