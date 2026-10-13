"""Explicit crossing junctions and independent connected-component/cycle counts."""

from itertools import pairwise

import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
from shapely import get_parts, hausdorff_distance, linestrings, unary_union


def topology(positions, edges):
    edges = np.asarray(edges, dtype=np.int32).reshape(-1, 2)
    rows, cols = edges.T
    matrix = coo_matrix(
        (np.ones(len(edges)), (rows, cols)), shape=(len(positions), len(positions))
    ).tocsr()
    count = connected_components(matrix, directed=False, return_labels=False)
    return {
        "components": int(count),
        "cycles": int(len(edges) - len(positions) + count),
    }


def node_network(positions, edges):
    original = linestrings(positions[edges])
    union = unary_union(original)
    points, keys, emitted = [], {}, set()
    for part in get_parts(union):
        indices = []
        for coordinate in part.coords:
            key = tuple(coordinate)
            if key not in keys:
                keys[key] = len(points)
                points.append(coordinate)
            indices.append(keys[key])
        emitted.update(tuple(sorted(pair)) for pair in pairwise(indices))
    points, emitted = np.array(points), np.array(sorted(emitted), dtype=np.int32)
    if len(points) > 8192 or len(emitted) > 16384:
        raise ValueError("noded vein graph exceeds export budget")
    rebuilt = unary_union(linestrings(points[emitted]))
    error = float(hausdorff_distance(union, rebuilt))
    assert error <= 1e-14
    return points, emitted, topology(points, emitted) | {"union_error_m": error}
