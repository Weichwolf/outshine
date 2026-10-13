"""Close a validated conforming transport surface with vertical sides and a bottom."""

import numpy as np

from ..contracts import Mesh


def _sorted_boundary(triangles):
    edges = triangles[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    keys = np.sort(edges, axis=1)
    _, first, inverse, counts = np.unique(keys, axis=0, return_index=True, return_inverse=True, return_counts=True)
    if np.any(counts > 2):
        raise ValueError("nonmanifold surface edge")
    directed = np.where(edges[:, 0] < edges[:, 1], 1, -1)
    sums = np.bincount(inverse, weights=directed, minlength=len(counts))
    if np.any(sums[counts == 2] != 0):
        raise ValueError("shared surface edge has inconsistent orientation")
    return edges[first[counts == 1]]


def _orient_surface(vertices, triangles):
    triangles = np.array(triangles, dtype=np.uint32, copy=True)
    points = vertices[triangles, :2]
    a, b = points[:, 1] - points[:, 0], points[:, 2] - points[:, 0]
    cross = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    if np.any(cross == 0):
        raise ValueError("surface has zero projected area")
    reverse = cross < 0
    triangles[reverse] = triangles[reverse, ::-1]
    return triangles


def _close_surface(surface, depth_m, boundary):
    if not np.isfinite(depth_m) or depth_m <= 0:
        raise ValueError("positive finite vertical depth required")
    if not len(surface.triangles):
        raise ValueError("surface must contain triangles")
    triangles = _orient_surface(surface.vertices, surface.triangles)
    edges = boundary(triangles)
    starts, a = np.unique(edges[:, 0], return_counts=True)
    ends, b = np.unique(edges[:, 1], return_counts=True)
    if not (np.array_equal(starts, ends) and np.all(a == 1) and np.all(b == 1)):
        raise ValueError("boundary must be disjoint closed loops")
    count = len(surface.vertices)
    if count > np.iinfo(np.uint32).max // 2:
        raise ValueError("closed body exceeds uint32 vertex capacity")
    if depth_m > np.finfo(np.float64).max + min(0., float(surface.vertices[:, 2].min())):
        raise ValueError("bottom height would overflow")
    bottom = surface.vertices.copy()
    bottom[:, 2] -= depth_m
    if not np.all(bottom[:, 2] < surface.vertices[:, 2]):
        raise ValueError("vertical depth is not representable at these heights")
    a, b = edges.T
    sides = np.stack((np.c_[a, a + count, b], np.c_[b, a + count, b + count]), axis=1).reshape(-1, 3)
    return Mesh(np.vstack((surface.vertices, bottom)),
                np.vstack((triangles, triangles[:, ::-1] + count, sides)))


def close_surface(surface: Mesh, depth_m: float) -> Mesh:
    """Close a conforming, nonoverlapping XY surface using its explicit XYZ heights.

    The caller must already have verified the projected geometry, including holes
    and absence of T junctions. This operation checks edge incidence, boundary
    loops and nonzero projected faces; it does not solve heights or traffic rules.
    Vertices are retained once across top interfaces. Bottom heights are lowered
    vertically by depth_m; outer and hole boundaries receive outward facing walls.
    """
    if not isinstance(surface, Mesh):
        raise ValueError("one owned Mesh is required")
    return _close_surface(surface, depth_m, _sorted_boundary)
