#!/usr/bin/env python3
"""Inspect owned landing ports against native road-height cycles."""

import argparse
import collections
import heapq
import json
import math
from pathlib import Path
import time


def reaches(line, end, crossing, limit):
    ordered = list(reversed(line)) if end else line
    reached = 0.0
    for first, second in zip(ordered, ordered[1:]):
        a = (first['eastM'], first['northM'])
        b = (second['eastM'], second['northM'])
        delta = tuple(y - x for x, y in zip(a, b))
        squared = sum(x * x for x in delta)
        if squared == 0:
            continue
        t = max(0.0, min(1.0, sum((p - x) * d for p, x, d in
                                 zip(crossing, a, delta)) / squared))
        projection = tuple(x + t * d for x, d in zip(a, delta))
        span = math.sqrt(squared)
        if math.dist(projection, crossing) <= .02 and reached + t * span <= limit:
            return True
        reached += span
        if reached > limit:
            break
    return False


def landing_aliases(context, reach):
    aliases = {}
    matched = []
    def root(node):
        while node in aliases and aliases[node] != node:
            node = aliases[node]
        return node
    for cross in context['crossings']:
        a, b = (context['ways'][cross[key]] for key in ('lane0', 'lane1'))
        if a['bridge'] != b['bridge'] or a['layer'] != b['layer']:
            continue
        nodes = (root(cross['node0']), root(cross['node1']))
        owner = min(nodes)
        for node in nodes:
            aliases[node] = owner
    for cross in context['crossings']:
        lanes = (cross['lane0'], cross['lane1'])
        ways = [context['ways'][i] for i in lanes]
        if (ways[0]['bridge'] == ways[1]['bridge'] or
                ways[0]['traffic'] != ways[1]['traffic']):
            continue
        lines = [context['designed'][i] for i in lanes]
        if any(len(line) < 2 for line in lines):
            continue
        position = (cross['eastM'], cross['northM'])
        owners = []
        for a in range(2):
            for b in range(2):
                if not ways[0]['ownedEnds'][a] or not ways[1]['ownedEnds'][b]:
                    continue
                first = lines[0][-1 if a else 0]['node']
                second = lines[1][-1 if b else 0]['node']
                if first == 0 or root(first) != root(second):
                    continue
                if all(reaches(line, end, position, reach) for line, end in zip(lines, (a, b))):
                    owners.append(root(first))
        if not owners:
            continue
        nodes = [root(cross['node0']), root(cross['node1']), *owners]
        owner = min(nodes)
        for node in nodes:
            aliases[node] = owner
        matched.append(dict(cross, owner=owner))
    return aliases, matched


def inspect(context, graph, aliases):
    def root(node):
        while node in aliases and aliases[node] != node:
            node = aliases[node]
        return node
    canonical = {}
    names = {i: root(j['node']) for i, j in enumerate(context['junctions'])}
    link = 0
    for edge in context['edges']:
        for at in range(edge['plannedFirst'], edge['plannedFirst'] + edge['plannedCount'] - 1):
            for station, key in ((at, 'first'), (at + 1, 'second')):
                node = context['stations'][station]['node']
                if station == edge['plannedFirst']:
                    node = edge['nodeAt'][0]
                elif station == edge['plannedFirst'] + edge['plannedCount'] - 1:
                    node = edge['nodeAt'][1]
                if node:
                    names[graph['links'][link][key]] = root(node)
            link += 1
    for index in range(len(graph['nodes'])):
        key = ('named', names[index]) if index in names else ('anonymous', index)
        canonical.setdefault(key, len(canonical))
    mapped = [canonical[('named', names[i]) if i in names else ('anonymous', i)]
              for i in range(len(graph['nodes']))]
    adjacency = collections.defaultdict(list)
    discarded = 0
    for index, link in enumerate(graph['links']):
        a, b = mapped[link['first']], mapped[link['second']]
        rise = link['maximumRiseM']
        adjacency[a].append((b, rise, 'link', index))
        adjacency[b].append((a, rise, 'link', index))
    for index, gap in enumerate(graph['clearances']):
        a, b = mapped[gap['upper']], mapped[gap['lower']]
        if a == b:
            discarded += 1
        else:
            adjacency[a].append((b, -gap['minimumGapM'], 'clearance', index))
    values = [0.0] * len(canonical)
    prior = [None] * len(canonical)
    queue = [(0.0, i) for i in range(len(canonical))]
    heapq.heapify(queue)
    began = time.monotonic()
    updates = 0
    while queue:
        value, node = heapq.heappop(queue)
        if value != values[node]:
            continue
        for child, cost, kind, index in adjacency[node]:
            candidate = value + cost
            if candidate >= values[child] - 1e-9:
                continue
            values[child] = candidate
            prior[child] = (node, cost, kind, index)
            updates += 1
            if cost < 0:
                chain, visited, at = [], {}, child
                for _ in range(128):
                    if at in visited:
                        cycle = chain[visited[at]:]
                        if sum(prior[n][1] for n in cycle) < -1e-8:
                            return dict(feasible=False, cycle=cycle, discarded=discarded,
                                        constraints=[prior[n] for n in cycle])
                        break
                    if prior[at] is None:
                        break
                    visited[at] = len(chain)
                    chain.append(at)
                    at = prior[at][0]
            heapq.heappush(queue, (candidate, child))
        if updates % 4096 == 0 and time.monotonic() - began > 10:
            raise TimeoutError('bounded height-cycle inspection')
    return dict(feasible=True, discarded=discarded)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('context', type=Path)
    parser.add_argument('heights', type=Path)
    parser.add_argument('--reach', type=float, default=4.0)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    context = json.loads(args.context.read_text())
    heights = json.loads(args.heights.read_text())
    began = time.monotonic()
    aliases, matched = landing_aliases(context, args.reach)
    result = dict(aliases=aliases, matched=matched, selectionSeconds=time.monotonic() - began,
                  untrimmedHeights=inspect(context, heights, aliases))
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: len(v) if isinstance(v, (dict, list)) and k != 'untrimmedHeights' else v
                      for k, v in result.items()}))


if __name__ == '__main__':
    main()
