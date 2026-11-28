"""Whole analytic and finite field checks on shared road/soil surfaces."""

import time

import numpy as np
from scipy import sparse

from ._field import control_bounds
from ._system import continuous_matrix, discrete_matrix, vertex_matrices


def normal_gradients(xy, faces, heights):
    vertices = np.c_[xy, heights][faces]
    normal = np.cross(vertices[:, 1] - vertices[:, 0], vertices[:, 2] - vertices[:, 0])
    return -normal[:, :2] / normal[:, 2:3]


def assemble_world(field, product, xy, faces, parts, frames, certificates=None):
    began = time.perf_counter()
    height, equality, _, vertex_parts = vertex_matrices(field, xy, faces, parts)
    road = parts >= 0
    if certificates is None:
        continuous, report = continuous_matrix(
            field,
            xy[faces[road]].reshape(-1, 2),
            np.arange(3 * road.sum()).reshape(-1, 3),
            parts[road],
            frames[road],
        )
    else:
        continuous, report = certificates
        report = dict(report)
    extra = height.shape[1] - continuous.shape[1]
    continuous = sparse.hstack(
        [continuous, sparse.csc_matrix((continuous.shape[0], extra))]
    ).tocsc()
    finite = discrete_matrix(xy, faces, frames, height)
    gradient = sparse.vstack([continuous, finite]).tocsc()
    report.update(
        plane_coefficients=3 * len(field.origins),
        soil_height_unknowns=extra,
        unknowns=height.shape[1],
        total_cones=gradient.shape[0] // 2,
        point_contact_equalities=equality.shape[0],
        world_faces=len(faces),
        assembly_ms=(time.perf_counter() - began) * 1000,
        sparse_bytes=sum(
            a.data.nbytes + a.indices.nbytes + a.indptr.nbytes
            for a in (height, gradient, equality)
        ),
    )
    return height, gradient, equality, vertex_parts, report


def approximation_bounds(field, coefficients, xy, faces, parts, frames):
    errors = np.zeros(len(faces))
    maximum = 0.0
    road = np.flatnonzero(parts >= 0)
    for begin in range(0, len(road), 256):
        ids = road[begin : begin + 256]
        grade, error = control_bounds(
            field, xy[faces[ids]], parts[ids], coefficients, frames[ids]
        )
        maximum = max(maximum, float(grade.max(initial=0)))
        errors[ids] = error
    return errors, maximum


def finite_ports(
    xy,
    faces,
    parts,
    heights,
    port_roots,
    roles,
    *,
    mark_limit=0.005,
    include_hard=False,
):
    selected = {edge: [] for edge in port_roots}
    for face, corners in enumerate(faces):
        for a, b in ((0, 1), (1, 2), (2, 0)):
            key = tuple(sorted((int(corners[a]), int(corners[b]))))
            if key in selected:
                selected[key].append(face)
    gradients = normal_gradients(xy, faces, heights)
    jumps, bad_faces, hard_bad = [], [], []
    by_port = np.zeros(max(port_roots.values(), default=-1) + 1)
    for edge, touching in selected.items():
        if len(touching) != 2 or any(parts[i] < 0 for i in touching):
            raise ValueError(
                "a whole port segment lost one of its two original road modules"
            )
        jump = float(np.linalg.norm(gradients[touching[0]] - gradients[touching[1]]))
        by_port[port_roots[edge]] = max(by_port[port_roots[edge]], jump)
        jumps.append(jump)
        if jump > mark_limit:
            bad_faces.extend(i for i in touching if roles[parts[i]] == 0)
        if jump > 0.005:
            hard_bad.extend(i for i in touching if roles[parts[i]] == 0)
    result = (
        np.unique(np.asarray(bad_faces, dtype=int)),
        {
            "original_ports": len(by_port),
            "current_shared_segments": len(jumps),
            "maximum_finite_contact_grade_jump": float(by_port.max(initial=0)),
            "p95_finite_contact_grade_jump": float(np.quantile(by_port, 0.95))
            if len(by_port)
            else 0.0,
            "original_ports_exceeding_0_005": int(np.count_nonzero(by_port > 0.005)),
        },
    )
    return (
        (*result, np.unique(np.asarray(hard_bad, dtype=int)))
        if include_hard
        else result
    )
