"""Independent explicit-polyhedron triangulation for procedural ray controls."""

import itertools

import numpy as np
from scipy.spatial import ConvexHull


def explicit_mesh(planes, centre):
    local = planes[:, 3] - planes[:, :3] @ centre
    triples = np.array(list(itertools.combinations(range(len(planes)), 3)))
    normal = planes[triples, :3]
    keep = np.abs(np.linalg.det(normal)) > 1e-10
    normal, triples = normal[keep], triples[keep]
    xyz = np.linalg.solve(normal, local[triples, ..., None])[..., 0]
    xyz = xyz[np.all(xyz @ planes[:, :3].T <= local + 1e-7, axis=1)]
    xyz = np.unique(np.round(xyz, 9), axis=0)
    hull = ConvexHull(xyz)
    triangles = xyz[hull.simplices]
    n = np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
    flipped = np.sum(n * hull.equations[:, :3], axis=1) < 0
    triangles[flipped] = triangles[flipped][:, [0, 2, 1]]
    return triangles + centre
