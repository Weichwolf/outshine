"""Link matching directed triangle edges while keeping separate point-contact fans."""

import numpy as np


def connected_fans(cached, order):
    properties = np.ascontiguousarray(np.c_[cached['vertices'], cached['uv']][cached['faces'][order]].reshape(-1, 5),
                                     dtype=np.float64)
    _, positions = np.unique(properties[:, :3], axis=0, return_inverse=True)
    parent = np.arange(len(properties))

    def root(index):
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    edges = {}
    for first in range(0, len(properties), 3):
        for a, b in ((first, first+1), (first+1, first+2), (first+2, first)):
            edges.setdefault(tuple(sorted((positions[a], positions[b]))), []).append((a, b))
    for group in edges.values():
        if len(group) == 2:
            (a, b), (c, d) = group
            if positions[a] != positions[d] or positions[b] != positions[c]:
                raise ValueError('inconsistent directed prism boundary')
            for start, end in ((a, d), (b, c)):
                start, end = root(start), root(end)
                parent[max(start, end)] = min(start, end)
    targets = np.array([root(i) for i in range(len(parent))], dtype=np.uint64)
    duplicate = np.flatnonzero(targets != np.arange(len(targets))).astype(np.uint64)
    return properties, np.arange(len(properties), dtype=np.uint64).reshape(-1, 3), duplicate, targets[duplicate]

