"""Noded transport axes remain separate from their shared physical footprints."""

from collections import defaultdict
import time

import numpy as np
import shapely
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components


def traffic_of(road):
    return 'rail' if road['properties'].get('class') in ('rail', 'transit') else 'land'


def missing_axes(lines, parts, tolerance):
    pairs = shapely.STRtree(parts).query(lines, predicate='dwithin', distance=tolerance)
    order = np.argsort(pairs[0], kind='stable')
    pairs = pairs[:, order]
    cuts = np.r_[0, np.flatnonzero(np.diff(pairs[0])) + 1, pairs.shape[1]]
    uncovered = sum(lines[i].length for i in set(range(len(lines))) - set(pairs[0]))
    for first, last in zip(cuts[:-1], cuts[1:]):
        source = int(pairs[0, first])
        local = shapely.union_all(parts[pairs[1, first:last]])
        covered = local.buffer(tolerance, resolution=2)
        uncovered += lines[source].difference(covered).length
    return float(uncovered)


def axes_of(roads, precision):
    groups = defaultdict(list)
    transitions = defaultdict(list)
    for index, road in enumerate(roads):
        key = (traffic_of(road), *road['tier'])
        xy = np.round(shapely.get_coordinates(road['line']) / precision) * precision
        line = shapely.LineString(xy)
        if line.length == 0:
            raise ValueError('source axis collapsed during endpoint normalization')
        groups[key].append((index, line))
        for point in xy[[0, -1]]:
            transitions[(key[0], *point)].append((index, key))
    graphs, stats = {}, []
    for tier, entries in groups.items():
        began = time.perf_counter()
        lines = np.array([line for _, line in entries], dtype=object)
        network = shapely.union_all(lines)
        parts = shapely.get_parts(network)
        coordinates = [shapely.get_coordinates(line) for line in parts]
        endpoints = np.array([xy[[0, -1]] for xy in coordinates]).reshape(-1, 2)
        vertices, indices = np.unique(endpoints, axis=0, return_inverse=True)
        indices = indices.reshape(-1, 2)
        graph = coo_matrix((np.ones(len(parts), dtype=np.uint8), indices.T),
                           shape=(len(vertices), len(vertices))).tocsr()
        count, component = connected_components(graph, directed=False)
        noding_ms = (time.perf_counter()-began)*1000
        began = time.perf_counter()
        uncovered = missing_axes(lines, parts, precision * 1e-4)
        audit_ms = (time.perf_counter()-began)*1000
        if uncovered > 1e-6:
            raise ValueError(f'{tier}: noding lost source axes')
        degree = np.bincount(indices.ravel(), minlength=len(vertices))
        graphs[tier] = dict(vertices=vertices, edges=indices, lines=parts,
                            source_indices=np.array([i for i, _ in entries]), component=component)
        stats.append(dict(tier=list(tier), source_axes=len(entries), vertices=len(vertices),
                          edges=len(indices), components=int(count),
                          branches=int(np.count_nonzero(degree > 2)),
                          terminal_nodes=int(np.count_nonzero(degree == 1)),
                          noding_ms=noding_ms,source_coverage_audit_ms=audit_ms,
                          preserved_axis_length_m=float(network.length),
                          uncovered_source_length_m=uncovered))
    transitions = [dict(position_m=list(point[1:]), traffic=point[0], sources=[i for i, _ in entries],
                        tiers=[list(tier) for tier in sorted({key for _, key in entries})])
                   for point, entries in transitions.items() if len({key for _, key in entries}) > 1]
    return graphs, stats, transitions


def verify():
    def road(xy, layer=0, bridge='ground', kind='minor'):
        return dict(line=shapely.LineString(xy), tier=(layer, bridge), properties={'class': kind})
    sources = [road([(-10, 0), (10, 0)]), road([(0, -10), (0, 10)]),
               road([(10, 0), (20, 0)], 1, 'bridge'),
               road([(-5, -5), (5, 5)], kind='rail')]
    graphs, _, transitions = axes_of(sources, .001)
    ground = graphs[('land', 0, 'ground')]
    assert len(ground['vertices']) == 5 and len(ground['edges']) == 4
    assert ground['component'].max() == 0 and len(graphs) == 3
    assert len(transitions) == 1 and transitions[0]['position_m'] == [10., 0.]
