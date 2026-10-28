from fractions import Fraction as F
import argparse
import json
from pathlib import Path
import time

from clip_round_witness import certifies, clip, convex, rounded, search, token

from canonical_polygons import load_exports, whole
from check_reference import fixture_paths
from shapely.geometry import Polygon, box


def main(output=None):
    checks = []
    def check(name, condition):
        assert condition, name
        checks.append(name)
    original, _ = load_exports(fixture_paths())
    point = (8744 * 4096 + 587, 5705 * 4096 + 42)
    pair = [o for o in original if point in list(o.polygon.exterior.coords)]
    check('manifest-checked original pair has two observations', len(pair) == 2)
    full = next(o for o in pair if whole(o))
    check('original pair has no unmodelled holes', all(not o.polygon.interiors for o in pair))
    complete = [(int(x), int(y)) for x, y in list(full.polygon.exterior.coords)[:-1]]
    observations = [(list(o.polygon.exterior.coords)[:-1], o.window) for o in pair]
    integer_witness = [tuple(F(v) for v in p) for p in complete]
    check('integer complete outline alone does not reproduce rounded clipped source',
          not certifies(integer_witness, observations))
    exact = [(F(x) + (F(1, 4) if x % 4096 in (649, 641) else 0), F(y)) for x, y in complete]
    check('one shared rational polygon reproduces BOTH complete original rings', certifies(exact, observations))
    check('shared witness has valid convex topology before any clipping', convex(exact))
    for expected, window in observations:
        from_exact = clip(exact, window)
        # GEOS uses floating intersections; compare its rounded output rather
        # than claiming its non-binary rational intersections remain exact.
        geos = Polygon([(float(x), float(y)) for x, y in exact]).intersection(box(*window))
        check('independent GEOS clip/round agrees with rational certificate ' + str(window),
              token(rounded(from_exact)) == token(rounded(list(geos.exterior.coords)[:-1])))
    old, new = (35816072, 23367744), (35816034, 23367744)
    bad = [([new if p == old else p for p in ring], window) for ring, window in observations]
    check('declared inside-envelope cut mutation is not certified by shared witness', not certifies(exact, bad))
    check('ring start and winding do not change exact certification',
          certifies(exact[1:] + exact[:1], [(r[::-1], w) for r, w in observations]))
    check('star polygon is rejected despite equal turn signs',
          not convex([(0, 3), (2, -3), (-3, 1), (3, 1), (-2, -3)]))
    check('half-open rounding respects negative and positive midpoints',
          rounded([(F(-1, 2), F(1, 2)), (F(1, 2), F(-1, 2)), (2, 2)]) == [(0, 1), (1, 0), (2, 2)])
    start = time.monotonic()
    found, attempts = search(complete, observations, attempts=1024)
    elapsed = time.monotonic() - start
    check('bounded search finds a separately certified shared original witness',
          found is not None and certifies(found, observations))
    report = {'scope': 'Conditional convex pointwise clip/round model only; not provider '
                      'deployment calibration, topology repair, identity or World admission.',
              'checks': checks, 'count': len(checks),
              'explicit_global_witness': [[str(x), str(y)] for x, y in exact],
              'bounded_search_attempts': attempts, 'bounded_search_seconds': elapsed,
              'bounded_search_witness': [[str(x), str(y)] for x, y in found]}
    if output is not None:
        output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Check conditional whole-polygon clip/round witnesses.')
    parser.add_argument('--output', type=Path, help='Optional report outside the fixed inputs.')
    main(parser.parse_args().output)
