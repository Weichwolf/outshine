"""Shared vertex placement with bounded labelled triangle correspondence."""

import numpy as np


def partition(positions, indices, radius):
    positions, indices = np.asarray(positions), np.asarray(indices)
    if (
        positions.dtype != np.float32
        or positions.ndim != 2
        or positions.shape[1] != 3
        or not 0 < len(positions) <= 262144
        or not np.isfinite(positions).all()
        or np.max(abs(positions)) > 1e6
        or indices.ndim != 2
        or indices.shape[1] != 3
        or indices.dtype.kind not in "iu"
        or not 0 < len(indices) <= 131072
        or indices.min() < 0
        or indices.max() >= len(positions)
        or not np.isfinite(radius)
        or radius <= 0
    ):
        raise ValueError("bounded finite Float32 mesh and positive radius required")
    step = np.nextafter(2 * radius / np.sqrt(3), 0)
    rounded = np.rint(positions.astype(float) / step)
    if not np.isfinite(rounded).all() or np.max(abs(rounded)) > np.iinfo(np.int32).max:
        raise ValueError("bounded integer vertex cluster coordinates required")
    cells, first, mapping, counts = np.unique(
        rounded.astype(np.int32),
        axis=0,
        return_index=True,
        return_inverse=True,
        return_counts=True,
    )
    return cells, first, mapping, counts, cells.astype(float) * step


def qef_direction(positions, indices, mapping, centres, active):
    triangle = positions[indices].astype(float)
    normal = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    outer = normal[:, :, None] * normal[:, None, :]
    matrices = np.zeros((len(centres), 3, 3))
    rhs = np.zeros((len(centres), 3))
    relations = 0
    for corner in range(3):
        ids = mapping[indices[:, corner]]
        use = active[ids]
        ids, n = ids[use], normal[use]
        distance = np.einsum("ij,ij->i", n, triangle[use, 0] - centres[ids])
        np.add.at(matrices, ids, outer[use])
        np.add.at(rhs, ids, n * distance[:, None])
        relations += len(ids)
    eigenvalues, vectors = np.linalg.eigh(matrices[active])
    threshold = np.maximum(eigenvalues[:, -1:], 0) * (64 * np.finfo(float).eps)
    good = (eigenvalues > threshold) & (eigenvalues > 0)
    inverse = np.divide(1, eigenvalues, out=np.zeros_like(eigenvalues), where=good)
    projected = np.einsum("nki,nk->ni", vectors, rhs[active])
    direction = np.zeros_like(centres)
    direction[active] = np.einsum("nki,ni->nk", vectors, projected * inverse)
    return direction, {
        "qef_corner_cluster_relations": relations,
        "qef_eigendecompositions": int(active.sum()),
        "qef_rank_counts": np.bincount(good.sum(1), minlength=4).tolist(),
        "qef_matrix_rhs_array_bytes": matrices.nbytes + rhs.nbytes,
    }


def feasible_segment(positions, mapping, centres, direction, radius):
    delta = direction[mapping]
    relative = centres[mapping] - positions.astype(float)
    a = np.einsum("ij,ij->i", delta, delta)
    b = 2 * np.einsum("ij,ij->i", delta, relative)
    c = np.minimum(np.einsum("ij,ij->i", relative, relative) - radius**2, 0)
    radical = np.sqrt(np.maximum(b * b - 4 * a * c, 0))
    root = np.full(len(mapping), np.inf)
    forward = (a > 0) & (b >= 0) & (b + radical > 0)
    root[forward] = -2 * c[forward] / (b[forward] + radical[forward])
    backward = (a > 0) & (b < 0)
    root[backward] = (-b[backward] + radical[backward]) / (2 * a[backward])
    boundary = (a > 0) & (b >= 0) & (b + radical == 0)
    root[boundary] = 0
    alpha = np.ones(len(centres))
    np.minimum.at(alpha, mapping, root)
    alpha = np.clip(alpha, 0, 1)
    return centres + alpha[:, None] * direction, alpha


def cluster_mesh(positions, indices, *, method, radius=3.0):
    if method not in ("centre", "qef"):
        raise ValueError("declared centre or qef placement required")
    cells, first, mapping, counts, centres = partition(positions, indices, radius)
    active = counts > 1
    work = {"qef_corner_cluster_relations": 0, "qef_eigendecompositions": 0}
    centre_error = np.linalg.norm(centres[mapping] - positions.astype(float), axis=1)
    if centre_error.max() > radius:
        raise ValueError("centre candidate lacks the declared floating-point radius")
    solver_failure = False
    if method == "qef":
        try:
            direction, fit_work = qef_direction(
                positions, indices, mapping, centres, active
            )
            placed, alpha = feasible_segment(
                positions, mapping, centres, direction, radius
            )
            work.update(fit_work)
        except np.linalg.LinAlgError:
            solver_failure = True
            placed, alpha = centres.copy(), np.ones(len(centres))
            work.update(
                qef_eigendecompositions=int(active.sum()),
                qef_corner_cluster_relations=int(active[mapping[indices]].sum()),
                qef_rank_counts=None,
            )
    else:
        placed, alpha = centres.copy(), np.ones(len(centres))
    placed[~active] = positions[first[~active]]
    floating_error = np.linalg.norm(placed[mapping] - positions.astype(float), axis=1)
    bad = (floating_error > radius) | ~np.isfinite(floating_error)
    fallback = np.unique(mapping[bad])
    if solver_failure:
        fallback = np.flatnonzero(active)
    placed[fallback] = centres[fallback]
    floating_error = np.linalg.norm(placed[mapping] - positions.astype(float), axis=1)
    if floating_error.max() > radius or not np.isfinite(floating_error).all():
        raise ValueError("actual original-vertex placement exceeds declared radius")
    packed = placed.astype(np.float32)
    words, remap = np.unique(packed.view(np.uint32), axis=0, return_inverse=True)
    final = words.copy().view(np.float32)
    vertex_map = remap[mapping]
    index_type = np.uint16 if len(final) <= 65536 else np.uint32
    faces = vertex_map[indices].astype(index_type)
    before = positions[indices].astype(float)
    after = final[faces].astype(float)
    old_normal = np.cross(before[:, 1] - before[:, 0], before[:, 2] - before[:, 0])
    new_normal = np.cross(after[:, 1] - after[:, 0], after[:, 2] - after[:, 0])
    swap = np.einsum("ij,ij->i", old_normal, new_normal) < 0
    faces[swap] = faces[swap][:, [0, 2, 1]]
    rounding = float(np.linalg.norm(packed.astype(float) - placed, axis=1).max())
    actual = np.linalg.norm(
        final[vertex_map].astype(float) - positions.astype(float), axis=1
    )
    if actual.max() > radius + rounding:
        raise ValueError("packed vertex shift exceeds placement plus measured rounding")
    return {
        "positions": final,
        "indices": faces,
        "corner_swap": swap,
        "input_vertex_map": vertex_map.astype(np.uint32),
    }, {
        "method": method,
        "radius_m": radius,
        "cluster_step_m": float(np.nextafter(2 * radius / np.sqrt(3), 0)),
        "clusters": len(cells),
        "unchanged_singleton_clusters": int((~active).sum()),
        "vertices": len(final),
        "triangles": len(indices),
        "input_vertex_cluster_relations": len(mapping),
        "maximum_prepack_vertex_shift_m": float(floating_error.max()),
        "maximum_float32_rounding_m": rounding,
        "maximum_stored_vertex_shift_m": float(actual.max()),
        "geometric_model_displacement_bound_m": radius + rounding,
        "numerical_centre_fallback_clusters": len(fallback),
        "qef_solver_failure": solver_failure,
        "clipped_qef_clusters": int(((alpha < 1) & active).sum())
        if method == "qef"
        else 0,
        "reoriented_triangles": int(swap.sum()),
        "zero_area_triangles": int((np.linalg.norm(new_normal, axis=1) == 0).sum()),
        "geometry_bytes": final.nbytes + faces.nbytes,
        **work,
    }
