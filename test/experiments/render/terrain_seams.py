"""Audit cached rendered height pages; retain actual heights and native sampling fractions."""

import argparse
import csv
from collections import defaultdict
import hashlib
import json
from pathlib import Path

import numpy as np


def mixed_edges(lookup):
    maximum_zoom = max(tile[0] for tile in lookup)
    groups = defaultdict(lambda: ([], []))
    for (z, x, y), (row, height) in lookup.items():
        length = 1 << (maximum_zoom-z)
        low_x, low_y = x*length, y*length
        fractions = np.loadtxt(Path(row['directory'])/(row['page']+'.fractions'))
        for axis, coordinate, low, side, values in (
                (0, low_x, low_y, 0, height[:, 0]),
                (0, low_x+length, low_y, 1, height[:, -1]),
                (1, low_y, low_x, 0, height[0, :]),
                (1, low_y+length, low_x, 1, height[-1, :])):
            groups[(axis, coordinate)][side].append((low, low+length, (z,x,y),
                                                    low+fractions*length, values))
    seams = []
    for (axis, coordinate), (left, right) in groups.items():
        for edges in (left, right):
            edges.sort(key=lambda edge: edge[0])
            if any(a[1] > b[0] for a, b in zip(edges, edges[1:])):
                raise ValueError('overlapping terrain coverage requires a different edge sweep')
        a = b = 0
        while a < len(left) and b < len(right):
            first, second = left[a], right[b]
            low, high = max(first[0],second[0]), min(first[1],second[1])
            if low < high and first[2][0] != second[2][0]:
                samples = np.unique(np.r_[first[3],second[3],low,high])
                samples = samples[(samples >= low)&(samples <= high)]
                one = np.interp(samples,first[3],first[4])
                two = np.interp(samples,second[3],second[4])
                error = abs(one-two)
                seams.append(dict(tile=list(first[2]), neighbour=list(second[2]),axis=axis,
                                  mismatch_m=float(error.max()),samples=len(samples)))
            if first[1] <= second[1]:a += 1
            else:b += 1
    seams.sort(key=lambda edge: edge['mismatch_m'],reverse=True)
    return seams


def analyse(directory):
    pages = list(csv.DictReader((directory / 'pages.csv').open()))
    lookup = {}
    for row in pages:
        row['directory'] = str(directory)
        side, halo = int(row['side']), int(row['halo'])
        path = directory / (row['page']+'.f32')
        data = np.fromfile(path, dtype='<f4')
        if len(data) != int(row['nodes']) or not np.isfinite(data).all():
            raise ValueError('incomplete page export')
        fractions = np.loadtxt(directory / (row['page']+'.fractions'))
        if (fractions.shape != (side,) or not np.isfinite(fractions).all()
                or fractions[0] != 0 or fractions[-1] != 1
                or not np.all(np.diff(fractions) > 0)):
            raise ValueError('invalid native sampling fractions')
        height = data.reshape(side+2*halo, side+2*halo)[halo:halo+side, halo:halo+side]
        tile = tuple(int(row[key]) for key in ('zoom', 'x', 'y'))
        if tile in lookup:
            raise ValueError('duplicate rendered tile')
        lookup[tile] = (row, height)
    if not lookup:
        raise ValueError('empty terrain export')
    seams = []
    for (z, x, y), (row, height) in lookup.items():
        for axis, neighbour, a in ((0, (z, x+1, y), height[:, -1]),
                                   (1, (z, x, y+1), height[-1, :])):
            if neighbour not in lookup:
                continue
            _, other = lookup[neighbour]
            b = other[:, 0] if axis == 0 else other[0, :]
            if a.shape != b.shape:
                raise ValueError('same-level pages disagree on sampling size')
            error = abs(a-b)
            seams.append(dict(tile=[z, x, y], neighbour=list(neighbour), axis=axis,
                              mismatch_m=float(error.max()), node=int(error.argmax())))
    seams.sort(key=lambda row: row['mismatch_m'], reverse=True)
    mixed = mixed_edges(lookup)
    return dict(pages=len(pages), checked_same_level_edges=len(seams),
                checked_mixed_level_edges=len(mixed),worst_mixed_level_edges=mixed[:30],
                worst_same_level_edges=seams[:30],
                scope='native height samples and piecewise linear mixed-LOD edges; shader placement and visibility remain unproved',
                pages_sha256=hashlib.sha256((directory / 'pages.csv').read_bytes()).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    report = analyse(args.directory)
    (args.directory / 'seams.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(pages=report['pages'], edges=report['checked_same_level_edges'],
                          mixed=report['checked_mixed_level_edges'],
                          mixed_worst_m=report['worst_mixed_level_edges'][0]['mismatch_m'] if report['worst_mixed_level_edges'] else None,
                          worst_m=report['worst_same_level_edges'][0]['mismatch_m'] if report['worst_same_level_edges'] else None)))


if __name__ == '__main__':
    main()
