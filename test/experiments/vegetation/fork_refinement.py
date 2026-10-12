"""Bounded projection and shared-edge subdivision of implicit fork surface meshes."""

import numpy as np
from fork_surface import blocked_voxel_mesh, mesh_product


def project_vertices(field, vertices, maximum_motion):
    result = vertices.copy()
    evaluations = 0
    tolerance = field.ray_extent**2 * 1e-12
    for _ in range(6):
        values, gradient = field.evaluate(result, True)
        evaluations += len(result)
        if np.max(abs(values)) <= tolerance:
            break
        squared = np.sum(gradient**2, axis=1)
        if np.any(squared <= 0) or not np.isfinite(squared).all():
            raise ValueError("nonzero finite surface gradient required for projection")
        result -= (values / squared)[:, None] * gradient
        if np.max(np.linalg.norm(result - vertices, axis=1)) > maximum_motion:
            raise ValueError(
                "surface projection exceeded its local displacement budget"
            )
    residual = float(np.max(abs(field.evaluate(result))))
    evaluations += len(result)
    if not np.isfinite(result).all() or residual > tolerance:
        raise ValueError("bounded surface projection did not converge")
    return result, evaluations, residual


def subdivide(vertices, faces):
    if len(faces) * 4 > 1_000_000:
        raise ValueError("subdivision exceeds the million-triangle experiment budget")
    edges, inverse = np.unique(
        np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1),
        axis=0,
        return_inverse=True,
    )
    mids = inverse.reshape(-1, 3) + len(vertices)
    vertices = np.vstack((vertices, np.mean(vertices[edges], axis=1)))
    a, b, c = faces.T
    ab, bc, ca = mids.T
    faces = np.vstack(
        (
            np.column_stack((a, ab, ca)),
            np.column_stack((ab, b, bc)),
            np.column_stack((ca, bc, c)),
            np.column_stack((ab, bc, ca)),
        )
    )
    return vertices, faces


def refined_voxel_mesh(field, cells=128, rounds=1):
    if type(rounds) is not int or not 0 <= rounds <= 2:
        raise ValueError("zero through two bounded shared-edge subdivisions required")
    mesh, work = blocked_voxel_mesh(field, cells, 8)
    vertices, faces = mesh["vertices"], mesh["faces"]
    evaluations = 0
    maximum_motion = 2 * field.ray_extent / cells
    for phase in range(rounds + 1):
        if phase:
            vertices, faces = subdivide(vertices, faces)
        vertices, count, residual = project_vertices(field, vertices, maximum_motion)
        evaluations += count
    return mesh_product(vertices, faces, field), work | {
        "method": "refined",
        "subdivision_rounds": rounds,
        "projection_point_evaluations": evaluations,
        "maximum_vertex_field_residual_m2": residual,
        "projection_motion_budget_m": maximum_motion,
    }
