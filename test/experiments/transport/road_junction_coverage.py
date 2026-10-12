#!/usr/bin/env python3
"""Compare junction coverage from a JSON array of cached MVT street features."""

import collections
import json
import math
import sys


def cross(a, b):
    return a[0] * b[1] - a[1] * b[0]


def area(ring):
    return abs(sum(cross(a, b) for a, b in zip(ring, ring[1:] + ring[:1]))) / 2


def footprint(gates):
    centre = tuple(sum(g[axis] for g in gates) / len(gates) for axis in (0, 1))
    corners = [(e + n * 3 * hand, y - x * 3 * hand, i)
               for i, (e, y, x, n) in enumerate(gates) for hand in (1, -1)]
    corners.sort(key=lambda p: (math.atan2(p[1] - centre[1], p[0] - centre[0]), p[2]))
    return centre, corners


def old_ring(centre, corners, legs):
    corners = list(corners)
    while len(corners) > 3:
        for i, b in enumerate(corners):
            a, c = corners[i - 1], corners[(i + 1) % len(corners)]
            if cross((b[0] - a[0], b[1] - a[1]), (c[0] - b[0], c[1] - b[1])) <= 0:
                corners.pop(i)
                break
        else:
            break
    if legs == 2:
        pairs = [(a, b) for a in corners for b in corners if a[2] != b[2]]
        a, b = max(pairs, key=lambda p: sum((p[0][i] - p[1][i]) ** 2 for i in (0, 1)))
        return [centre, a, b]
    return corners


def evaluate(directions):
    gates = [(8 * e, 8 * n, e, n) for e, n in directions]
    centre, corners = footprint(gates)
    full = area(corners)
    fan = [cross((a[0] - centre[0], a[1] - centre[1]),
                 (b[0] - centre[0], b[1] - centre[1])) / 2
           for a, b in zip(corners, corners[1:] + corners[:1])]
    assert min(fan) >= -1e-8 and abs(sum(fan) - full) < 1e-8
    return full, area(old_ring(centre, corners, len(gates)))


def main(path):
    rows = json.load(open(path))
    nodes = collections.defaultdict(set)
    for row in rows:
        points = row['points']
        for a, b in zip(points, points[1:]):
            for start, end in ((a, b), (b, a)):
                key = tuple(round(p, 7) for p in start)
                delta = ((end[1] - start[1]) * math.cos(math.radians(start[0])),
                         end[0] - start[0])
                length = math.hypot(*delta)
                if length > 0:
                    nodes[key].add(tuple(round(p / length, 8) for p in delta))
    compared = []
    for position, directions in nodes.items():
        if len(directions) < 2:
            continue
        full, old = evaluate(sorted(directions))
        if full > 1e-8:
            compared.append(dict(position=position, legs=len(directions),
                                 footprint_m2=full, old_m2=old, coverage=old / full))
    for degrees in (0, 30, 60, 90):
        turn = math.radians(degrees)
        full, old = evaluate([(1, 0), (-math.cos(turn), math.sin(turn))])
        print(json.dumps(dict(case=f'two-leg-{degrees}', footprint_m2=full, old_m2=old)))
    print(json.dumps(dict(nodes=len(compared),
                          two_leg=sum(r['legs'] == 2 for r in compared),
                          lost_coverage=sum(r['coverage'] < .999 for r in compared))))
    print(json.dumps(sorted(compared, key=lambda r: r['coverage'])[:5]))


if __name__ == '__main__':
    main(sys.argv[1])
