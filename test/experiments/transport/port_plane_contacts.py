"""Whole original road interfaces: structural C1 proof and independent numeric checks."""

import numpy as np
from surface_height_checks import normal_gradients


def contact_audit(field, coefficients, product, heights):
    roles, pairs = product["part_roles"], product["port_parts"]
    road = np.where(roles[pairs[:, 0]] == 0, pairs[:, 0], pairs[:, 1])
    core = np.where(roles[pairs[:, 0]] == 1, pairs[:, 0], pairs[:, 1])
    ends = product["vertices"][product["port_edges"]]
    common = field.plane_ids[core, 0]
    plane = field.plane_ids[road]
    blended = plane[:, 0] != plane[:, 1]
    distance = np.linalg.norm(
        field.cut_centres[road] - ends.mean(axis=1)[:, None], axis=2
    )
    side = np.argmin(distance, axis=1)
    index = np.arange(len(road))
    if np.any(plane[index, side] != common) or np.any(
        distance[index, side][blended] > 1e-8
    ):
        raise ValueError(
            "shared plane identity/cut line does not cover a complete original port"
        )
    normals = field.inward_normals[road, side]
    error = abs(
        np.einsum("nki,ni->nk", ends - field.cut_centres[road, side][:, None], normals)
    )
    if error[blended].max(initial=0) > 1e-8:
        raise ValueError("original whole port is not on its planned parent cut")
    height_error, grade_error = np.zeros(len(road)), np.zeros(len(road))
    weights = np.linspace(0, 1, 17)
    for begin in range(0, len(road), 256):
        stop = min(begin + 256, len(road))
        points = (
            (1 - weights[None, :, None]) * ends[begin:stop, 0, None]
            + weights[None, :, None] * ends[begin:stop, 1, None]
        ).reshape(-1, 2)
        left, gl = field.evaluate(points, np.repeat(road[begin:stop], 17), coefficients)
        right, gr = field.evaluate(
            points, np.repeat(core[begin:stop], 17), coefficients
        )
        height_error[begin:stop] = abs(left - right).reshape(-1, 17).max(axis=1)
        grade_error[begin:stop] = (
            np.linalg.norm(gl - gr, axis=1).reshape(-1, 17).max(axis=1)
        )
    if height_error.max(initial=0) > 1e-7 or grade_error.max(initial=0) > 1e-10:
        raise ValueError("numeric complete-port C1 contact audit exceeded tolerance")
    gradients = normal_gradients(product["vertices"], product["indices"], heights)
    selected = {tuple(sorted(e)): [] for e in product["port_edges"]}
    for face, corners in enumerate(product["indices"]):
        for a, b in ((0, 1), (1, 2), (2, 0)):
            key = tuple(sorted((corners[a], corners[b])))
            if key in selected:
                selected[key].append(face)
    jumps = np.array(
        [
            np.linalg.norm(gradients[ids[0]] - gradients[ids[1]])
            for ids in selected.values()
        ]
    )
    return {
        "complete_original_ports": len(road),
        "analytic_c1_by_shared_plane_identity": True,
        "maximum_cut_line_distance_m": float(error[blended].max(initial=0)),
        "maximum_contact_height_error_m": float(height_error.max(initial=0)),
        "maximum_contact_grade_error": float(grade_error.max(initial=0)),
        "original_unrefined_finite_gradient_jump_maximum": float(jumps.max(initial=0)),
        "original_unrefined_finite_gradient_jump_p95": float(np.quantile(jumps, 0.95))
        if len(jumps)
        else 0.0,
        "original_unrefined_finite_ports_exceeding_0_005": int(
            np.count_nonzero(jumps > 0.005)
        ),
    }
