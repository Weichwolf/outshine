"""Conforming oriented children retain their original whole source domains."""

import numpy as np
import shapely


def topology_audit(original, mesh):
    faces = mesh["faces"]
    edges = np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]])
    unique, inverse, counts = np.unique(
        np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    winding = np.bincount(inverse, weights=np.where(edges[:, 0] < edges[:, 1], 1, -1))
    if np.any(counts > 2) or np.any(winding[counts == 2] != 0):
        raise ValueError(
            "sliced world has nonmanifold or inconsistently oriented edges"
        )
    old_faces = original["faces"]
    old_edges = np.concatenate(
        [old_faces[:, [0, 1]], old_faces[:, [1, 2]], old_faces[:, [2, 0]]]
    )
    old_unique, old_counts = np.unique(
        np.sort(old_edges, axis=1), axis=0, return_counts=True
    )
    expected = old_unique[old_counts == 1]
    boundary = unique[counts == 1]
    old_lines = shapely.union_all(shapely.linestrings(original["xy"][expected]))
    new_lines = shapely.union_all(shapely.linestrings(mesh["xy"][boundary]))
    tolerance = 64 * np.finfo(float).eps * max(1, abs(original["xy"]).max())
    boundary_vertices, degree = np.unique(boundary, return_counts=True)
    mapped = mesh["original_vertex_map"][expected]
    old_vertices, old_degree = np.unique(mapped, return_counts=True)
    expected_degree = np.full(len(mesh["xy"]), 2)
    expected_degree[old_vertices] = old_degree
    if (
        np.any(degree != expected_degree[boundary_vertices])
        or shapely.hausdorff_distance(old_lines, new_lines) > tolerance
        or abs(old_lines.length - new_lines.length) > tolerance * max(1, len(boundary))
    ):
        raise ValueError(
            "sliced world changed the original outer boundary or has a T-junction"
        )
    new_corners = mesh["xy"][faces]
    a, b = new_corners[:, 1] - new_corners[:, 0], new_corners[:, 2] - new_corners[:, 0]
    new_area = 0.5 * (a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0])
    old_corners = original["xy"][old_faces]
    a, b = old_corners[:, 1] - old_corners[:, 0], old_corners[:, 2] - old_corners[:, 0]
    old_area = 0.5 * (a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0])
    root_area = np.bincount(mesh["roots"], weights=new_area, minlength=len(old_faces))
    error = float(abs(root_area - old_area).max())
    if error > 1e-6 or np.any(new_area <= 0):
        raise ValueError("sliced world changed resolvable source areas")
    return {
        "outer_edges": len(boundary),
        "original_outer_edges": len(expected),
        "maximum_root_area_error_m2": error,
    }
