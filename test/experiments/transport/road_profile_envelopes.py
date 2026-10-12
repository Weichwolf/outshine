#!/usr/bin/env python3
"""Compare DEM-following road heights with joint grade-constrained graph envelopes.

Input: lane, station, east, north, height, node, max_gradient, bridge, layer.
Ground roads only. Optional hard bounds pin established contacts; no junction-plane/C1 proof.
"""
import argparse
import csv
import hashlib
import heapq
import json
import math
from pathlib import Path
import random
import shlex
import subprocess
import tempfile
import time


def envelope(targets, adjacency):
    heights = list(targets)
    queue = [(height, node) for node, height in enumerate(heights) if math.isfinite(height)]
    heapq.heapify(queue)
    depth = [0] * len(heights)
    while queue:
        height, node = heapq.heappop(queue)
        if height != heights[node]:
            continue
        for neighbour, rise in adjacency[node]:
            candidate = height + rise
            if candidate < heights[neighbour]:
                depth[neighbour] = depth[node] + 1
                if depth[neighbour] >= len(heights):
                    raise ValueError('attachment offsets conflict with permitted gradients')
                heights[neighbour] = candidate
                heapq.heappush(queue, (candidate, neighbour))
    return heights


def fit(samples, adjacency, bounds=None, reverse=None, prefer_cuts=False):
    bounds = bounds or [(-math.inf, math.inf)] * len(samples)
    if reverse is None:
        reverse = [[] for _ in samples]
        for node, links in enumerate(adjacency):
            for neighbour, cost in links:
                reverse[neighbour].append((node, cost))
    minimum = [-x for x in envelope([-low for low, high in bounds], reverse)]
    maximum = envelope([high for low, high in bounds], adjacency)
    if any(low > high + 1e-9 for low, high in zip(minimum, maximum)):
        raise ValueError('hard contact heights conflict with permitted road gradients')
    if prefer_cuts:
        caps = [min(high, max(low, min(values)))
                for values, low, high in zip(samples, minimum, maximum)]
        heights = envelope(caps, adjacency)
        correction = max(abs(height - sample)
                         for height, values in zip(heights, samples) for sample in values)
        return heights, correction
    upper = envelope([min(values) for values in samples], adjacency)
    lower = [-x for x in envelope([-max(values) for values in samples], reverse)]
    correction = max(0, max((low - high) / 2 for low, high in zip(lower, upper)),
                     max(low - high for low, high in zip(minimum, upper)),
                     max(low - high for low, high in zip(lower, maximum)))
    heights = [max(max(floor, low - correction),
                   min(min(ceiling, high + correction), (low + high) / 2))
               for low, high, floor, ceiling in zip(lower, upper, minimum, maximum)]
    return heights, correction



def build_native(directory):
    root = Path(__file__).resolve().parents[2]
    source = root / 'src/generators/road/RoadHeightPlan.cpp'
    database = root / 'compile_commands.json'
    if not database.exists():
        raise SystemExit('Run make db before the native experiment.')
    entry = next((item for item in json.loads(database.read_text())
                  if Path(item['file']).resolve() == source), None)
    if entry is None:
        raise SystemExit('Refresh make db: the native road height unit is missing.')
    arguments = entry.get('arguments') or shlex.split(entry['command'])
    flags = []
    remaining = iter(arguments[1:])
    for argument in remaining:
        if argument in ('-o', '-MF', '-MT', '-MQ'):
            next(remaining)
        elif argument not in (str(source), '-c', '-MMD', '-MD', '-MP'):
            flags.append(argument)
    runner = directory / 'main.cpp'
    runner.write_text(r'''
#include "RoadHeightPlan.h"
#include <chrono>
#include <cstdio>
#include <vector>
int main() {
  using namespace outshine::Generators;
  unsigned count = 0, links = 0, gaps = 0, cuts = 0;
  while (std::scanf("%u%u%u%u", &count, &links, &gaps, &cuts) == 4) {
    std::vector<RoadHeightNode> nodes(count);
    std::vector<RoadHeightLink> edges(links);
    std::vector<RoadHeightClearance> clearances(gaps);
    for (auto &node : nodes) {
      if (std::scanf("%lf%lf%lf%lf", &node.LowSampleM, &node.HighSampleM,
                     &node.MinimumM, &node.MaximumM) != 4) { return 2; }
    }
    for (auto &edge : edges) {
      if (std::scanf("%u%u%lf%lf%lf", &edge.First, &edge.Second, &edge.MaximumRiseM,
                     &edge.FirstOffsetM, &edge.SecondOffsetM) != 5) { return 2; }
    }
    for (auto &gap : clearances) {
      if (std::scanf("%u%u%lf%lf%lf", &gap.Lower, &gap.Upper, &gap.MinimumGapM,
                     &gap.LowerOffsetM, &gap.UpperOffsetM) != 5) { return 2; }
    }
    const auto began = std::chrono::steady_clock::now();
    const auto plan = PlanRoadHeights(nodes, edges, clearances,
        cuts ? RoadHeightFit::PreferCuts : RoadHeightFit::MinimaxAdjustment);
    const double ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - began).count();
    if (!plan) {
      std::printf("ERR\t%.*s\n", int(plan.error().Reason.size()), plan.error().Reason.data());
      continue;
    }
    std::printf("OK\t%.17g\t%.17g\n", plan->MaximumAdjustmentM, ms);
    for (const double height : plan->HeightM) { std::printf("%.17g ", height); }
    std::putchar('\n');
  }
}
''')
    binary = directory / 'road-heights'
    subprocess.run([arguments[0], *flags, str(runner), str(source), '-o', str(binary)],
                   cwd=entry['directory'], check=True, capture_output=True, text=True, timeout=60)
    digest = hashlib.sha256(source.read_bytes() + source.with_suffix('.h').read_bytes()).hexdigest()
    toolchain = hashlib.sha256(json.dumps(arguments).encode()).hexdigest()
    return binary, digest, toolchain


def run_native(binary, cases, prefer_cuts=False):
    rows = []
    for samples, edges, bounds, *extra in cases:
        gaps = extra[0] if extra else []
        rows.append(f'{len(samples)} {len(edges)} {len(gaps)} {int(prefer_cuts)}')
        rows.extend(' '.join(map(str, (min(values), max(values), *bound)))
                    for values, bound in zip(samples, bounds))
        rows.extend(' '.join(map(str, edge)) for edge in edges)
        rows.extend(' '.join(map(str, gap)) for gap in gaps)
    process = subprocess.run([str(binary)], input='\n'.join(rows) + '\n', capture_output=True,
                             text=True, check=True, timeout=60)
    lines = iter(process.stdout.splitlines())
    results = []
    for samples, *_ in cases:
        header = next(lines).split('\t')
        if header[0] == 'ERR':
            results.append(dict(status='ERR', error=header[1]))
            continue
        assert header[0] == 'OK' and len(header) == 3, header
        heights = list(map(float, next(lines).split()))
        assert len(heights) == len(samples)
        results.append(dict(status='OK', correction=float(header[1]),
                            ms=float(header[2]), heights=heights))
    assert next(lines, None) is None
    return results


def verify_native_offsets(binary):
    import numpy as np
    from scipy.optimize import linprog

    randomizer = random.Random(228144)
    cases, answers = [], []
    for trial in range(180):
        count = randomizer.randrange(3, 24)
        baseline = [randomizer.uniform(-10, 10) for _ in range(count)]
        samples = [[randomizer.uniform(-80, 160) for _ in range(randomizer.randrange(1, 4))]
                   for _ in range(count)]
        edges, gaps, constraints = [], [], []
        for first in range(count):
            for second in range(first + 1, count):
                if second != first + 1 and randomizer.random() > .12:
                    continue
                left, right = [randomizer.uniform(-8, 8) for _ in range(2)]
                rise = abs(baseline[first] + left - baseline[second] - right)
                rise += randomizer.uniform(0, 8)
                edges.append((first, second, rise, left, right))
                for sign in (-1, 1):
                    coefficients = np.zeros(count + 1)
                    coefficients[first], coefficients[second] = sign, -sign
                    constraints.append((coefficients, rise - sign * (left - right)))
        if trial % 2:
            ordered = sorted(range(count), key=baseline.__getitem__)
            for lower, upper in zip(ordered, ordered[1:]):
                left, right = [randomizer.uniform(-2, 2) for _ in range(2)]
                space = baseline[upper] + right - baseline[lower] - left
                if space < 0:
                    continue
                gap = randomizer.uniform(0, space)
                gaps.append((lower, upper, gap, left, right))
                coefficients = np.zeros(count + 1)
                coefficients[lower], coefficients[upper] = 1, -1
                constraints.append((coefficients, -gap + right - left))
        for node, values in enumerate(samples):
            for sample in values:
                for sign in (-1, 1):
                    coefficients = np.zeros(count + 1)
                    coefficients[node], coefficients[-1] = sign, -1
                    constraints.append((coefficients, sign * sample))
        bounds = [(-math.inf, math.inf)] * count
        for node in range(count):
            if trial < 60 or randomizer.random() > .4:
                continue
            centre = baseline[node]
            if trial >= 120:
                centre += randomizer.uniform(-100, 100)
            width = randomizer.uniform(0, 3) if randomizer.random() < .5 else 0
            bounds[node] = (centre - width, centre + width)
        objective = np.zeros(count + 1)
        objective[-1] = 1
        answer = linprog(objective, A_ub=[row for row, limit in constraints],
                         b_ub=[limit for row, limit in constraints],
                         bounds=[(None if math.isinf(low) else low,
                                  None if math.isinf(high) else high) for low, high in bounds]
                         + [(0, None)], method='highs')
        assert answer.success or answer.status == 2, answer.message
        cases.append((samples, edges, bounds, gaps))
        answers.append(answer)
    feasible = 0
    difference = 0
    cut_difference = 0
    cut_answers = run_native(binary, cases, prefer_cuts=True)
    for trial, (case, native, answer) in enumerate(zip(cases, run_native(binary, cases), answers)):
        if not answer.success:
            assert native['status'] == 'ERR', (trial, native)
            assert cut_answers[trial]['status'] == 'ERR', (trial, cut_answers[trial])
            continue
        assert native['status'] == 'OK', (trial, native)
        samples, edges, bounds, gaps = case
        heights = native['heights']
        difference = max(difference, abs(native['correction'] - answer.fun))
        assert abs(native['correction'] - answer.fun) < 1e-7, trial
        assert all(abs(heights[a] + left - heights[b] - right) <= rise + 1e-7
                   for a, b, rise, left, right in edges), trial
        assert all(heights[upper] + right - heights[lower] - left >= gap - 1e-7
                   for lower, upper, gap, left, right in gaps), trial
        assert all(low - 1e-7 <= height <= high + 1e-7
                   for height, (low, high) in zip(heights, bounds)), trial
        actual = max(abs(height - sample) for height, values in zip(heights, samples)
                     for sample in values)
        assert abs(actual - native['correction']) < 1e-7, trial
        adjacency, reverse = [[[] for _ in samples] for _ in range(2)]
        cut_constraints = []
        for a, b, rise, left, right in edges:
            adjacency[a].append((b, rise + left - right))
            adjacency[b].append((a, rise - left + right))
            reverse[a].append((b, rise - left + right))
            reverse[b].append((a, rise + left - right))
            for sign in (-1, 1):
                coefficients = np.zeros(len(samples))
                coefficients[a], coefficients[b] = sign, -sign
                cut_constraints.append((coefficients, rise - sign * (left - right)))
        for lower, upper, gap, left, right in gaps:
            adjacency[upper].append((lower, -gap + right - left))
            reverse[lower].append((upper, -gap + right - left))
            coefficients = np.zeros(len(samples))
            coefficients[lower], coefficients[upper] = 1, -1
            cut_constraints.append((coefficients, -gap + right - left))
        floors = [-x for x in envelope([-low for low, high in bounds], reverse)]
        caps = [min(high, max(low, min(values)))
                for values, low, (_, high) in zip(samples, floors, bounds)]
        cut_program = linprog(-np.ones(len(samples)),
                              A_ub=[row for row, limit in cut_constraints],
                              b_ub=[limit for row, limit in cut_constraints],
                              bounds=[(None if math.isinf(low) else low, cap)
                                      for (low, high), cap in zip(bounds, caps)], method='highs')
        cut_native = cut_answers[trial]
        assert cut_program.success and cut_native['status'] == 'OK', trial
        cut_difference = max(cut_difference, max(abs(a - b)
                             for a, b in zip(cut_program.x, cut_native['heights'])))
        assert cut_difference < 1e-7, trial
        feasible += 1
    return dict(cases=len(cases), feasible=feasible, infeasible=len(cases) - feasible,
                cases_with_clearances=sum(bool(case[3]) for case in cases),
                maximum_objective_difference_m=difference,
                maximum_cut_height_difference_m=cut_difference)


def solve(rows, pinned=None, native=None):
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
    report = dict(nodes=len(nodes), segments=len(edges), grounded_lanes=len(chains),
                  maximum_height_change_m=correction, unavoidable_change_m=unavoidable,
                  maximum_grade_violation_m=max(0, violation),
                  pinned_nodes=sum(low == high for low, high in bounds),
                  original_violating_segments=sum(
                      max(abs(x - y) for x in samples[a] for y in samples[b]) > rise + 1e-9
                      for a, b, rise in edges))
    cut_heights, cut_change = fit(samples, adjacency, bounds, prefer_cuts=True)
    report['earthwork'] = {}
    for name, candidate in [('minimax', heights), ('prefer_cuts', cut_heights)]:
        changes = [height - sample for height, values in zip(candidate, samples)
                   for sample in values]
        report['earthwork'][name] = dict(raised_samples=sum(x > .01 for x in changes),
                                        changed_samples=sum(abs(x) > .01 for x in changes),
                                        mean_absolute_change_m=sum(map(abs, changes)) / len(changes),
                                        maximum_change_m=max(map(abs, changes)))
    cut_violation = max((abs(cut_heights[a] - cut_heights[b]) - rise
                         for a, b, rise in edges), default=0)
    assert cut_violation < 1e-9
    report['earthwork']['prefer_cuts']['maximum_grade_violation_m'] = max(0, cut_violation)
    if native:
        result = run_native(native, [(samples, [(*edge, 0, 0) for edge in edges], bounds)])[0]
        assert result['status'] == 'OK', result
        difference = max(abs(a - b) for a, b in zip(heights, result['heights']))
        assert difference < 1e-7 and abs(unavoidable - result['correction']) < 1e-7
        report['native'] = dict(solve_ms=result['ms'], maximum_height_difference_m=difference)
        cut = run_native(native, [(samples, [(*edge, 0, 0) for edge in edges], bounds)],
                         prefer_cuts=True)[0]
        assert cut['status'] == 'OK', cut
        difference = max(abs(a - b) for a, b in zip(cut_heights, cut['heights']))
        assert difference < 1e-7 and abs(cut_change - cut['correction']) < 1e-7
        report['earthwork']['prefer_cuts']['native'] = dict(solve_ms=cut['ms'],
                                                         maximum_height_difference_m=difference)
    return report, heights, chains


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
    parser.add_argument('--native', action='store_true', help='verify and time the production C++ solver')
    args = parser.parse_args()
    with args.profiles.open() as source:
        rows = list(csv.reader(source))
    with tempfile.TemporaryDirectory(prefix='outshine-road-heights-') as temporary:
        native, digest, toolchain = build_native(Path(temporary)) if args.native else (None, None, None)
        began = time.perf_counter()
        report, heights, chains = solve(rows, native=native)
        report['solve_ms'] = (time.perf_counter() - began) * 1000
        if native:
            report['native']['source_sha256'] = digest
            report['native']['toolchain_sha256'] = toolchain
            report['native_offset_linear_programs'] = verify_native_offsets(native)
    report['scope'] = ('grounded graph secants; hard bounds, port offsets and directed clearance LP; '
                       'no whole-deck or junction-plane/C1 proof')
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
