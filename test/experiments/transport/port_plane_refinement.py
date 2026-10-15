"""Conforming red/green refinement inside unchanged source triangles, including soil."""

import numpy as np


def children(corners, mids):
    a, b, c = corners
    ab, bc, ca = mids
    mask = int(ab >= 0) + 2 * int(bc >= 0) + 4 * int(ca >= 0)
    if mask == 0:
        return [(a, b, c)]
    if mask == 1:
        return [(a, ab, c), (ab, b, c)]
    if mask == 2:
        return [(b, bc, a), (bc, c, a)]
    if mask == 4:
        return [(c, ca, b), (ca, a, b)]
    if mask == 3:
        return [(b, bc, ab), (a, ab, c), (ab, bc, c)]
    if mask == 6:
        return [(c, ca, bc), (b, bc, a), (bc, ca, a)]
    if mask == 5:
        return [(a, ab, ca), (c, ca, b), (ca, ab, b)]
    return [(a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca)]


def refine(
    xy,
    faces,
    parts,
    frames,
    targets,
    roots,
    port_roots,
    marked,
    *,
    heights=None,
    max_vertices=131072,
    max_faces=262144,
):
    if marked.shape != (len(faces),) or marked.dtype != bool:
        raise ValueError("one explicit boolean refinement choice per triangle required")
    if len(xy) > max_vertices or len(faces) > max_faces:
        return None, {"valid": False, "reason": "refinement_input_budget"}
    all_edges = np.sort(
        np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]), axis=1
    )
    edges, inverse = np.unique(all_edges, axis=0, return_inverse=True)
    wanted = np.zeros(len(edges), bool)
    np.logical_or.at(wanted, inverse, np.tile(marked, 3))
    midpoint = (xy[edges[:, 0]] + xy[edges[:, 1]]) * 0.5
    possible = np.any(midpoint != xy[edges[:, 0]], axis=1) & np.any(
        midpoint != xy[edges[:, 1]], axis=1
    )
    selected = np.flatnonzero(wanted & possible)
    if not len(selected):
        return None, {"valid": False, "reason": "no_representable_refinement_edge"}
    if len(xy) + len(selected) > max_vertices:
        return None, {"valid": False, "reason": "refinement_vertex_budget"}
    midpoint_ids = np.full(len(edges), -1, int)
    midpoint_ids[selected] = len(xy) + np.arange(len(selected))
    mids = midpoint_ids[inverse].reshape(3, -1).T
    counts = 1 + (mids >= 0).sum(axis=1)
    if counts.sum() > max_faces:
        return None, {"valid": False, "reason": "refinement_face_budget"}
    new_faces = np.concatenate(
        [np.asarray(children(f, m), int) for f, m in zip(faces, mids)]
    )
    new_xy = np.vstack([xy, midpoint[selected]])
    p = new_xy[new_faces]
    first, second = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    area = first[:, 0] * second[:, 1] - first[:, 1] * second[:, 0]
    if np.any(area <= 0):
        return None, {"valid": False, "reason": "numeric_refinement_degeneracy"}
    source = xy[faces]
    u, v = source[:, 1] - source[:, 0], source[:, 2] - source[:, 0]
    old_area = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]
    parent = np.repeat(np.arange(len(faces)), counts)
    reconstructed = np.bincount(parent, weights=area, minlength=len(faces))
    error = float(abs(reconstructed - old_area).sum()) * 0.5
    tolerance = 256 * np.finfo(float).eps * max(1, abs(old_area).sum() * 0.5)
    if error > tolerance:
        return None, {
            "valid": False,
            "reason": "refinement_source_area_changed",
            "error_m2": error,
        }
    updated_ports = dict(port_roots)
    for edge, vertex in zip(edges[selected], midpoint_ids[selected]):
        key = tuple(map(int, edge))
        if key in updated_ports:
            root = updated_ports.pop(key)
            for endpoint in edge:
                updated_ports[tuple(sorted((int(endpoint), int(vertex))))] = root
    new_targets = np.r_[targets, targets[edges[selected]].mean(axis=1)]
    result = {
        "xy": new_xy,
        "faces": new_faces,
        "parts": parts[parent],
        "frames": frames[parent],
        "targets": new_targets,
        "roots": roots[parent],
        "port_roots": updated_ports,
    }
    if heights is not None:
        result["heights"] = np.r_[heights, heights[edges[selected]].mean(axis=1)]
    report = {
        "valid": True,
        "reason": "",
        "added_vertices": len(selected),
        "faces_before": len(faces),
        "faces_after": len(new_faces),
        "marked_faces": int(marked.sum()),
        "skipped_unrepresentable_edges": int(np.count_nonzero(wanted & ~possible)),
        "source_area_error_m2": error,
        "target_policy": "Linear interpolation of the same captured coarse DEM-prior field, not newly observed source heights.",
    }
    return result, report
