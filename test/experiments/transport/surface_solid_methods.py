"""Dictionary reference versus the shared vectorized body-closing implementation."""

import numpy as np

from outshine_ref.transport.surface import _close_surface, _orient_surface, _sorted_boundary

sorted_boundary = _sorted_boundary
orient_surface = _orient_surface


def dictionary_boundary(triangles):
    groups = {}
    for row in triangles:
        for a, b in ((row[0], row[1]), (row[1], row[2]), (row[2], row[0])):
            a, b = int(a), int(b)
            key = (min(a, b), max(a, b))
            groups.setdefault(key, []).append((a, b))
    output = []
    for key in sorted(groups):
        edges = groups[key]
        if len(edges) > 2:
            raise ValueError("nonmanifold surface edge")
        if len(edges) == 2:
            if edges[0] != edges[1][::-1]:
                raise ValueError("shared surface edge has inconsistent orientation")
        else:
            output.append(edges[0])
    return np.asarray(output, dtype=np.uint32).reshape(-1, 2)



def close_surface(surface, depth_m, boundary=sorted_boundary):
    return _close_surface(surface, depth_m, boundary)
