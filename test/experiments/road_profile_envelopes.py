#!/usr/bin/env python3
"""Compare DEM-following road heights with joint grade-constrained graph envelopes.

Input: lane, station, east, north, height, node, max_gradient, bridge, layer.
Ground roads only. This experiment does not solve bridges, junction planes or C1 derivatives.
"""
import argparse
import csv
import heapq
import json
import math
from pathlib import Path
import random
import time


def envelope(targets, adjacency):
    heights = list(targets)
    queue = [(height, node) for node, height in enumerate(heights)]
    heapq.heapify(queue)
    while queue:
        height, node = heapq.heappop(queue)
        if height != heights[node]:
            continue
        for neighbour, rise in adjacency[node]:
            candidate = height + rise
            if candidate < heights[neighbour]:
                heights[neighbour] = candidate
                heapq.heappush(queue, (candidate, neighbour))
    return heights


def solve(rows):
    nodes, samples, chains = {}, [], {}
    for row in rows:
        lane, station, east, north, height, node, grade, bridge, layer = row
        if int(bridge) or float(grade) <= 0:
            continue
        key = ('node', int(node)) if int(node) else ('station', int(lane), int(station))
        if key not in nodes:
            nodes[key] = len(samples)
            samples.append([])
        index = nodes[key]
        samples[index].append(float(height))
        chains.setdefault(int(lane), []).append(
            (int(station), index, float(east), float(north), float(grade)))
    if not nodes:
        raise ValueError('no grounded road profiles with positive grade limits')
    adjacency = [[] for _ in samples]
    edges = []
    for chain in chains.values():
        chain.sort()
        for left, right in zip(chain, chain[1:]):
            distance = math.hypot(right[2] - left[2], right[3] - left[3])
            rise = min(left[4], right[4]) * distance
            if distance <= 0:
                raise ValueError('degenerate road segment')
            adjacency[left[1]].append((right[1], rise))
            adjacency[right[1]].append((left[1], rise))
            edges.append((left[1], right[1], rise))
    lower = envelope([min(values) for values in samples], adjacency)
    upper = [-x for x in envelope([-max(values) for values in samples], adjacency)]
    heights = [(low + high) / 2 for low, high in zip(lower, upper)]
    unavoidable = max(high - low for low, high in zip(lower, upper)) / 2
    correction = max(abs(height - sample)
                     for height, values in zip(heights, samples) for sample in values)
    violation = max((abs(heights[a] - heights[b]) - rise for a, b, rise in edges), default=0)
    assert violation < 1e-9
    assert abs(correction - unavoidable) < 1e-9
    return dict(nodes=len(nodes), segments=len(edges), grounded_lanes=len(chains),
                maximum_height_change_m=correction, unavoidable_change_m=unavoidable,
                maximum_grade_violation_m=max(0, violation),
                original_violating_segments=sum(
                    max(abs(x - y) for x in samples[a] for y in samples[b]) > rise + 1e-9
                    for a, b, rise in edges)), heights, chains


def verify_linear_programs():
    import numpy as np
    from scipy.optimize import linprog

    randomizer = random.Random(2281)
    for trial in range(40):
        count = randomizer.randrange(3, 24)
        targets = [randomizer.uniform(-80, 160) for _ in range(count)]
        points = [(randomizer.uniform(-30, 30), randomizer.uniform(-30, 30))
                  for _ in range(count)]
        rows, constraints = [], []
        lane = 0
        for first in range(count):
            for second in range(first + 1, count):
                if second != first + 1 and randomizer.random() > .12:
                    continue
                grade = randomizer.uniform(.05, .25)
                distance = math.dist(points[first], points[second])
                for station, node in enumerate((first, second)):
                    rows.append((lane, station, *points[node], targets[node], node + 1,
                                 grade, 0, 0))
                for sign in (-1, 1):
                    coefficients = np.zeros(count + 1)
                    coefficients[first], coefficients[second] = sign, -sign
                    constraints.append((coefficients, grade * distance))
                lane += 1
        for node, target in enumerate(targets):
            for sign in (-1, 1):
                coefficients = np.zeros(count + 1)
                coefficients[node], coefficients[-1] = sign, -1
                constraints.append((coefficients, sign * target))
        objective = np.zeros(count + 1)
        objective[-1] = 1
        answer = linprog(objective, A_ub=[x for x, y in constraints],
                         b_ub=[y for x, y in constraints],
                         bounds=[(None, None)] * count + [(0, None)], method='highs')
        assert answer.success
        report, _, _ = solve(rows)
        assert abs(report['maximum_height_change_m'] - answer.fun) < 1e-7, trial
    return 40


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profiles', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--verify', action='store_true', help='compare with SciPy/HiGHS solutions')
    args = parser.parse_args()
    with args.profiles.open() as source:
        rows = list(csv.reader(source))
    began = time.perf_counter()
    report, heights, chains = solve(rows)
    report['solve_ms'] = (time.perf_counter() - began) * 1000
    report['scope'] = 'grounded graph secants; no bridge pins, junction planes or C1 proof'
    if args.verify:
        report['independent_linear_program_cases'] = verify_linear_programs()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    with args.output.with_suffix('.csv').open('w') as output:
        writer = csv.writer(output)
        for lane, chain in sorted(chains.items()):
            for station, node, east, north, grade in chain:
                writer.writerow((lane, station, heights[node]))
    print(json.dumps(report))


if __name__ == '__main__':
    main()
