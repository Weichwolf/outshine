#!/usr/bin/env python3
"""Compare DEM-following road heights with joint grade-constrained graph envelopes.

Input: lane, station, east, north, height, node, max_gradient, bridge, layer.
Ground roads only. Optional hard bounds pin established contacts; no junction-plane/C1 proof.
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


def fit(samples, adjacency, bounds=None):
    bounds = bounds or [(-math.inf, math.inf)] * len(samples)
    upper = envelope([min(values) for values in samples], adjacency)
    lower = [-x for x in envelope([-max(values) for values in samples], adjacency)]
    minimum = [-x for x in envelope([-low for low, high in bounds], adjacency)]
    maximum = envelope([high for low, high in bounds], adjacency)
    if any(low > high + 1e-9 for low, high in zip(minimum, maximum)):
        raise ValueError('hard contact heights conflict with permitted road gradients')
    correction = max(0, max((low - high) / 2 for low, high in zip(lower, upper)),
                     max(low - high for low, high in zip(minimum, upper)),
                     max(low - high for low, high in zip(lower, maximum)))
    heights = [(max(floor, low - correction) + min(ceiling, high + correction)) / 2
               for low, high, floor, ceiling in zip(lower, upper, minimum, maximum)]
    return heights, correction


def solve(rows, pinned=None):
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
    pinned = pinned or {}
    bounds = [pinned.get(key, (-math.inf, math.inf)) for key in nodes]
    heights, unavoidable = fit(samples, adjacency, bounds)
    correction = max(abs(height - sample)
                     for height, values in zip(heights, samples) for sample in values)
    violation = max((abs(heights[a] - heights[b]) - rise for a, b, rise in edges), default=0)
    assert violation < 1e-9
    assert abs(correction - unavoidable) < 1e-9
    assert all(low - 1e-9 <= height <= high + 1e-9
               for height, (low, high) in zip(heights, bounds))
    return dict(nodes=len(nodes), segments=len(edges), grounded_lanes=len(chains),
                maximum_height_change_m=correction, unavoidable_change_m=unavoidable,
                maximum_grade_violation_m=max(0, violation),
                pinned_nodes=sum(low == high for low, high in bounds),
                original_violating_segments=sum(
                    max(abs(x - y) for x in samples[a] for y in samples[b]) > rise + 1e-9
                    for a, b, rise in edges)), heights, chains


def verify_linear_programs():
    import numpy as np
    from scipy.optimize import linprog

    randomizer = random.Random(2281)
    feasible = 0
    for trial in range(120):
        count = randomizer.randrange(3, 24)
        targets = [randomizer.uniform(-80, 160) for _ in range(count)]
        points = [(randomizer.uniform(-30, 30), randomizer.uniform(-30, 30))
                  for _ in range(count)]
        rows, constraints, pinned = [], [], {}
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
        for node in range(count):
            if trial < 40 or randomizer.random() > .2:
                continue
            baseline = .03 * sum(points[node])
            if trial >= 80:
                baseline += randomizer.uniform(-100, 100)
            width = 0 if randomizer.random() < .5 else randomizer.uniform(0, 2)
            pinned[('node', node + 1)] = (baseline - width, baseline + width)
        objective = np.zeros(count + 1)
        objective[-1] = 1
        answer = linprog(objective, A_ub=[x for x, y in constraints],
                         b_ub=[y for x, y in constraints],
                         bounds=[pinned.get(('node', node + 1), (None, None))
                                 for node in range(count)] + [(0, None)], method='highs')
        try:
            report, _, _ = solve(rows, pinned)
            assert answer.success, trial
            assert abs(report['maximum_height_change_m'] - answer.fun) < 1e-7, trial
            feasible += 1
        except ValueError:
            assert answer.status == 2, trial
    return dict(cases=120, feasible=feasible, infeasible=120 - feasible)


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
    report['scope'] = 'grounded graph secants; hard contact bounds; no junction-plane/C1 proof'
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
