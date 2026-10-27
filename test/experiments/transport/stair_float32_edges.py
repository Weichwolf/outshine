"""Interior edge contractions for exact GPU XY aliases at one logical height."""

from collections import defaultdict

import numpy as np


def _step_links(around, steps, a, b):
    first = set(steps[np.any(around == a, axis=1)])
    second = set(steps[np.any(around == b, axis=1)])
    if first != second or min(first) < 0:
        return False
    for step in first:
        band = around[steps == step]
        shared = band[np.sum(np.isin(band, [a, b]), axis=1) == 2]
        neighbours = [
            set(band[np.any(band == v, axis=1)].ravel()) - {v} for v in (a, b)
        ]
        if (
            not 1 <= len(shared) <= 2
            or neighbours[0] & neighbours[1] != set(shared.ravel()) - {a, b}
            or len(set(band.ravel())) <= (3 if len(shared) == 1 else 4)
        ):
            return False
        edges = np.sort(band[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
        unique, counts = np.unique(edges, axis=0, return_counts=True)
        boundary = set(unique[counts == 1].ravel())
        if a in boundary and b in boundary and len(shared) != 1:
            return False
    return True


def _candidate(points, faces, owners, steps, a, b):
    membership = np.count_nonzero(np.isin(faces, [a, b]), axis=1)
    hit = np.flatnonzero(membership)
    around = faces[hit]
    shared = faces[membership == 2]
    if len(shared) != 2 or len(np.unique(owners[hit])) != 1:
        return None, "edge_or_source_boundary"
    if not _step_links(around, steps[hit], a, b):
        return None, "step_membership_or_topology"
    neighbours = [
        set(around[np.any(around == v, axis=1)].ravel()) - {v} for v in (a, b)
    ]
    if neighbours[0] & neighbours[1] != set(shared.ravel()) - {a, b}:
        return None, "link_condition"
    if len(set(around.ravel())) <= 4:
        return None, "small_vertex_star"
    edges = np.sort(around[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
    unique, counts = np.unique(edges, axis=0, return_counts=True)
    if np.any((counts == 1) & np.any(np.isin(unique, [a, b]), axis=1)):
        return None, "outer_boundary"
    tri = points[around]
    normals = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    gradients = -normals[:, :2] / normals[:, 2:3]
    residual = max(
        float(
            abs(
                tri[:, 0, 2]
                + np.einsum("ij,ij->i", points[v, :2] - tri[:, 0, :2], gradients)
                - points[v, 2]
            ).max()
        )
        for v in (a, b)
    )
    if residual > 1e-10:
        return None, "different_affine_height_planes"
    mapped = around.copy()
    mapped[mapped == b] = a
    kept = np.all(np.diff(np.sort(mapped, axis=1), axis=1) != 0, axis=1)
    polygons = points[mapped[kept], :2]
    delta_a, delta_b = polygons[:, 1] - polygons[:, 0], polygons[:, 2] - polygons[:, 0]
    if np.any(delta_a[:, 0] * delta_b[:, 1] - delta_a[:, 1] * delta_b[:, 0] <= 0):
        return None, "remaining_projected_orientation"
    return {"plane_residual_m": residual, "incident_faces": len(hit)}, None


def contract_alias_edges(points, faces, owners, steps, *, max_alias_pairs=64):
    if (
        points.ndim != 2
        or points.shape[1:] != (3,)
        or not 1 <= len(points) <= 65536
        or not np.isfinite(points).all()
        or abs(points).max() > 1e7
        or faces.ndim != 2
        or faces.shape[1:] != (3,)
        or not 1 <= len(faces) <= 8192
        or faces.dtype.kind not in "iu"
        or faces.min() < 0
        or faces.max() >= len(points)
        or owners.shape != (len(faces),)
        or steps.shape != owners.shape
        or owners.dtype.kind not in "iu"
        or steps.dtype.kind not in "iu"
        or np.any(owners <= 0)
        or np.any(steps < -1)
    ):
        raise ValueError(
            "bounded finite indexed cap and integer source/step ownership required"
        )
    if type(max_alias_pairs) is not int or not 0 <= max_alias_pairs <= 64:
        raise ValueError("bounded alias-pair work cap required")
    triangles = points[faces]
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    if np.any(normals[:, 2] <= 0):
        raise ValueError("positive projected input cap required before contraction")
    encoded = points[:, :2].astype(np.float32)
    groups = defaultdict(list)
    for vertex, (xy, point) in enumerate(zip(encoded, points, strict=True)):
        groups[(*xy, point[2])].append(vertex)
    aliases = [v for v in groups.values() if len(v) > 1]
    if sum(len(v) - 1 for v in aliases) > max_alias_pairs:
        raise ValueError("alias-pair budget exhausted before any edge contraction")
    note = {
        "input_vertices": len(points),
        "input_faces": len(faces),
        "alias_groups": len(aliases),
        "accepted_edges": 0,
        "removed_faces": 0,
        "faces_scanned": 0,
        "incident_faces": 0,
        "maximum_move_m": 0.0,
        "maximum_plane_residual_m": 0.0,
        "encoded_xy_bytes": encoded.nbytes,
        "selected_copy_bytes": 0,
        "refusals": {},
    }
    for members in aliases:
        if len(members) != 2:
            reason, candidate = "nonpair_alias_class", None
        else:
            a, b = members
            candidate, reason = _candidate(points, faces, owners, steps, a, b)
            note["faces_scanned"] += len(faces)
        if candidate is None:
            note["refusals"][reason] = note["refusals"].get(reason, 0) + 1
            continue
        mapped = faces.copy()
        mapped[mapped == b] = a
        kept = np.all(np.diff(np.sort(mapped, axis=1), axis=1) != 0, axis=1)
        next_faces, next_owners, next_steps = mapped[kept], owners[kept], steps[kept]
        note["selected_copy_bytes"] += (
            mapped.nbytes + next_faces.nbytes + next_owners.nbytes + next_steps.nbytes
        )
        note["accepted_edges"] += 1
        note["removed_faces"] += int((~kept).sum())
        note["incident_faces"] += candidate["incident_faces"]
        note["maximum_move_m"] = max(
            note["maximum_move_m"], float(np.linalg.norm(points[a] - points[b]))
        )
        note["maximum_plane_residual_m"] = max(
            note["maximum_plane_residual_m"], candidate["plane_residual_m"]
        )
        faces, owners, steps = next_faces, next_owners, next_steps
    if note["accepted_edges"]:
        used = np.unique(faces)
        remap = np.full(len(points), -1, dtype=np.intp)
        remap[used] = np.arange(len(used))
        points, faces = points[used], remap[faces]
        note["selected_copy_bytes"] += points.nbytes + faces.nbytes
    note.update(
        output_vertices=len(points),
        output_faces=len(faces),
        copy_scope="Accepted remap and final compaction arrays only; not all temporary allocations.",
        work_scope="One whole-cap face-membership query per candidate pair; incident_faces counts accepted vertex stars, not every NumPy scalar operation.",
        scope="Exact Float32 XY aliases at equal Float64 height, existing interior edge, one source mask, preserved membership and topology in every incident stair band, valid local links and affine planes. Remaining GPU XY coordinates are unchanged; caller still audits complete body, source coverage and actual Float32.",
    )
    return points, faces, owners, steps, note
