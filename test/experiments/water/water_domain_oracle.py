"""Complete pair/graph control; no tree, envelope components or disjoint-set implementation."""

from collections import defaultdict
from itertools import combinations

import shapely
from water_body_domains import merge_key


def all_pairs(polygons, inputs, sources, *, assume_ground):
    keys = [
        merge_key(info, source, assume_ground)[0]
        for info, source in zip(inputs, sources, strict=True)
    ]
    buckets = defaultdict(list)
    for index, key in enumerate(keys):
        if key is not None:
            buckets[key].append(index)
    neighbours = defaultdict(set)
    tests = 0
    for indices in buckets.values():
        for a, b in combinations(indices, 2):
            tests += 1
            cut = polygons[a].intersection(polygons[b])
            if cut.area > 0 or cut.length > 0:
                neighbours[a].add(b)
                neighbours[b].add(a)
    remaining = set(range(len(polygons)))
    groups = []
    while remaining:
        todo = [min(remaining)]
        group = set()
        while todo:
            at = todo.pop()
            if at in group:
                continue
            group.add(at)
            todo.extend(neighbours[at] - group)
        remaining -= group
        groups.append(tuple(sorted(group)))
    return tuple(sorted(groups)), {
        "relation_tests": tests,
        "all_source_pairs": len(polygons) * (len(polygons) - 1) // 2,
    }


def check_complete(polygons, bodies, expected):
    actual = tuple(body.sources for body in bodies)
    if actual != expected:
        raise ValueError("candidate graph differs from complete-pair BFS")
    for body in bodies:
        reference = shapely.union_all([polygons[i] for i in body.sources])
        if body.geometry.symmetric_difference(reference).area != 0:
            raise ValueError("candidate domain changes complete original coverage")
