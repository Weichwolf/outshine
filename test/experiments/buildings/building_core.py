"""Place fixed architectural core envelopes inside the supplied polygon, including holes."""

import math
import time

import numpy as np
import shapely


SERVICE_CORE_M = (2.4, 2.4)
MAX_CORE_CHOICES = 3


def dimensions(storeys, rise_m):
    if storeys == 1:
        return SERVICE_CORE_M
    risers = math.ceil(rise_m/.18)
    flight_treads = math.ceil(risers/2)-1
    depth = math.ceil((2*1.10+flight_treads*.28+.12)/.10)*.10
    return 3.0, depth


def placements(interior, root, storeys, rise_m):
    size = dimensions(storeys, rise_m)
    margin = interior.buffer(-.25, join_style='mitre')
    shapely.prepare(margin)
    a, b, c, d = interior.bounds
    results, comparison = [], []
    orientations = (size, size[::-1]) if size[0] != size[1] else (size,)
    for method in ('centres', 'bounded-grid'):
        began, tested, candidates = time.perf_counter(), 0, []
        for width, depth in orientations:
            if c-a < width+.50 or d-b < depth+.50:
                continue
            if method == 'centres':
                centres = [interior.centroid.coords[0], interior.representative_point().coords[0]]
            else:
                centres = [(x, y) for x in np.linspace(a+width/2+.25, c-width/2-.25, 9)
                           for y in np.linspace(b+depth/2+.25, d-depth/2-.25, 9)]
            for x, y in centres:
                tested += 1
                rectangle = shapely.box(x-width/2, y-depth/2, x+width/2, y+depth/2)
                if not margin.covers(rectangle) or rectangle.distance(root) < .86:
                    continue
                score = rectangle.distance(root)+.01*shapely.Point(x, y).distance(interior.centroid)
                candidates.append(dict(polygon=rectangle, method=method, score=score,
                                       dimensions_m=[width, depth], kind='stair-reservation' if storeys > 1 else 'service-reservation'))
        comparison.append(dict(method=method, candidates=tested, valid_placements=len(candidates),
                               elapsed_ms=(time.perf_counter()-began)*1000,
                               best_score=min((row['score'] for row in candidates), default=None)))
        results.extend(candidates)
    ordered = sorted(results, key=lambda row: (row['score'], row['polygon'].bounds, row['method']))
    unique = []
    for row in ordered:
        if any(row['polygon'].equals_exact(old['polygon'], 1e-8) for old in unique):
            continue
        unique.append(row)
    chosen = unique[:1]
    while len(chosen) < min(MAX_CORE_CHOICES, len(unique)):
        unused = [row for row in unique if all(row is not old for old in chosen)]
        chosen.append(max(unused, key=lambda row: (
            min(row['polygon'].centroid.distance(old['polygon'].centroid) for old in chosen), -row['score'])))
    return chosen, comparison
