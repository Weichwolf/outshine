#!/usr/bin/env python3
"""Compare sparse 2D infrastructure tiles with complete source-profile scans."""

import argparse
import collections
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics
import time


def load(path):
    lanes = collections.defaultdict(list)
    for row in csv.reader(path.open()):
        lanes[int(row[0])].append(dict(position=(float(row[2]), float(row[3])),
                                     node=int(row[5]), level=int(row[8]), bridge=int(row[7])))
    return lanes


def portions(first, last, size):
    cuts = {0.0, 1.0}
    delta = tuple(b - a for a, b in zip(first, last))
    for start, end, run in zip(first, last, delta):
        if run == 0:
            continue
        low, high = sorted((start, end))
        for line in range(math.floor(low / size) + 1, math.ceil(high / size)):
            cuts.add((line * size - start) / run)
    ordered = sorted(cuts)
    for start, end in zip(ordered, ordered[1:]):
        if end - start <= 1e-12:
            continue
        a = tuple(p + start * d for p, d in zip(first, delta))
        b = tuple(p + end * d for p, d in zip(first, delta))
        cell = tuple(math.floor((x + y) / (2 * size)) for x, y in zip(a, b))
        yield cell, a, b, start > 0, end < 1


def partition(lanes, size):
    tiles = collections.defaultdict(list)
    ports = collections.defaultdict(set)
    source_length = 0.0
    tiled_length = 0.0
    segments = 0
    began = time.perf_counter()
    for lane, rows in lanes.items():
        for ordinal, (first, last) in enumerate(zip(rows, rows[1:])):
            a, b = first['position'], last['position']
            source_length += math.dist(a, b)
            segments += 1
            for cell, begin, end, begin_port, end_port in portions(a, b, size):
                tiles[cell].append((lane, ordinal, begin, end))
                tiled_length += math.dist(begin, end)
                for position, shared in ((begin, begin_port), (end, end_port)):
                    if shared:
                        key = (lane, ordinal, *(round(p * 1000) for p in position),
                               first['level'], first['bridge'])
                        ports[key].add(cell)
    elapsed_ms = (time.perf_counter() - began) * 1000
    assert abs(tiled_length - source_length) < source_length * 1e-12
    assert all(len(cells) == 2 for cells in ports.values())
    return tiles, dict(tile_size_m=size, tiles=len(tiles), source_segments=segments,
                       tiled_segments=sum(map(len, tiles.values())), border_ports=len(ports),
                       length_error_m=tiled_length - source_length, python_partition_ms=elapsed_ms)


def nearby(tiles, centre, radius, size):
    low = [math.floor((p - radius) / size) for p in centre]
    high = [math.floor((p + radius) / size) for p in centre]
    return [part for x in range(low[0], high[0] + 1) for y in range(low[1], high[1] + 1)
            for part in tiles.get((x, y), ())]


def endpoint_levels(lanes):
    nodes = collections.defaultdict(list)
    for lane, rows in lanes.items():
        for point in (rows[0], rows[-1]):
            if point['node']:
                nodes[point['node']].append((lane, point['level'], point['bridge']))
    mixed = [ports for ports in nodes.values() if len({p[1:] for p in ports}) > 1]
    return dict(mixed_endpoint_positions=len(mixed),
                degrees=dict(collections.Counter(map(len, mixed))),
                consequence='Layer-only endpoint keys would disconnect these potential transitions; '
                            'coincidence alone does not establish a functional connection.')


def within_radius(parts, centre, radius):
    found = set()
    for lane, ordinal, first, last in parts:
        delta = tuple(b - a for a, b in zip(first, last))
        squared = sum(d * d for d in delta)
        along = max(0, min(1, sum((p - a) * d for p, a, d in zip(centre, first, delta)) /
                          squared)) if squared else 0
        closest = tuple(a + along * d for a, d in zip(first, delta))
        if math.dist(closest, centre) <= radius + 1e-9:
            found.add((lane, ordinal))
    return found


def measured_query(parts):
    durations = []
    for _ in range(7):
        began = time.perf_counter()
        found = within_radius(parts, (0, 0), 256)
        durations.append((time.perf_counter() - began) * 1000)
    return found, statistics.median(durations)


def verify():
    cases = (((-300, -300), (300, 300)), ((0, -700), (0, 700)),
             ((700, 0), (-700, 0)), ((-10, 256), (520, 256)), ((0, 0), (0, 0)))
    for first, last in cases:
        parts = list(portions(first, last, 256))
        assert abs(sum(math.dist(a, b) for _, a, b, _, _ in parts) - math.dist(first, last)) < 1e-9
        reverse = list(portions(last, first, 256))
        assert [cell for cell, *_ in parts] == [cell for cell, *_ in reversed(reverse)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profiles', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    verify()
    lanes = load(args.profiles)
    whole = [(lane, ordinal, a['position'], b['position']) for lane, rows in lanes.items()
             for ordinal, (a, b) in enumerate(zip(rows, rows[1:]))]
    expected, scan_ms = measured_query(whole)
    reports = []
    for size in (128, 256, 512, 1024):
        tiles, report = partition(lanes, size)
        selected = nearby(tiles, (0, 0), 256, size)
        actual, query_ms = measured_query(selected)
        assert actual == expected
        report.update(query_radius_m=256, query_candidates=len(selected),
                      exact_query_segments=len(expected), python_radius_scan_ms=scan_ms,
                      python_radius_query_ms=query_ms,
                      whole_scan_segments=report['source_segments'])
        reports.append(report)
    result = dict(input=str(args.profiles), input_sha256=hashlib.sha256(
        args.profiles.read_bytes()).hexdigest(), lanes=len(lanes), endpoint_levels=endpoint_levels(lanes),
        partitions=reports, scope='Local ENU prototype: exact source lines, sparse cells, shared '
        'border ports. No production geographic key, height-boundary/C1 or visibility proof.')
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
