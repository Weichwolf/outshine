"""Audit whole terrain/body interfaces and publish unresolved outer-edge obligations."""

import numpy as np
import shapely


def boundary_edges(faces):
    edges, counts = np.unique(
        np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1),
        axis=0,
        return_counts=True,
    )
    if np.any(counts > 2):
        raise ValueError("terrain has a nonmanifold edge")
    return edges[counts == 1]


def audit(
    body,
    original,
    original_faces,
    before,
    terrain,
    terrain_faces,
    refined,
    refined_faces,
    parents,
    matches,
):
    if refined[: len(body["vertices"])].tobytes() != body["vertices"].tobytes():
        raise ValueError("original body vertices changed")
    old_triangles = body["vertices"][body["faces"][parents]]
    normal = np.cross(
        old_triangles[:, 1] - old_triangles[:, 0],
        old_triangles[:, 2] - old_triangles[:, 0],
    )
    residual = (
        np.abs(
            np.einsum(
                "fvi,fi->fv", refined[refined_faces] - old_triangles[:, :1], normal
            )
        )
        / np.linalg.norm(normal, axis=1)[:, None]
    )
    tolerance = 128 * np.finfo(float).eps * max(1.0, float(np.abs(refined).max()))
    if residual.max(initial=0) > tolerance:
        raise ValueError("body subdivision leaves the original facet planes")
    outer_edges = boundary_edges(original_faces)
    outer_lines = shapely.linestrings(original[outer_edges, :2])
    outer = shapely.union_all(outer_lines)
    outer_tree = shapely.STRtree(outer_lines)
    chains = [body["shore_lower"], body["road_upper"]]
    interface = shapely.union_all(
        [shapely.LineString(body["vertices"][c, :2]) for c in chains]
    )
    edges = np.unique(
        np.sort(refined_faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1),
        axis=0,
    )
    body_edges = set(map(tuple, edges))
    contact_map = {int(row["terrain"]): int(row["body"]) for row in matches}
    checked = 0
    for a, b in boundary_edges(terrain_faces):
        middle = shapely.Point((terrain[a, :2] + terrain[b, :2]) / 2)
        if outer.distance(middle) <= tolerance:
            continue
        if (
            interface.distance(middle) > tolerance
            or a not in contact_map
            or b not in contact_map
        ):
            raise ValueError("unresolved bank-end or terrain boundary continuation")
        if tuple(sorted((contact_map[a], contact_map[b]))) not in body_edges:
            raise ValueError("terrain edge leaves a body T-junction")
        checked += 1
    lookup = {tuple(p[:2]): p for p in original}
    retained, changed = 0, 0
    for point in terrain:
        previous = lookup.get(tuple(point[:2]))
        if previous is not None:
            retained += 1
            changed += int(point.tobytes() != previous.tobytes())
    obligations = []
    for row in matches:
        node = int(row["terrain"])
        point = terrain[node, :2]
        if outer.distance(shapely.Point(point)) > tolerance:
            continue
        edge = int(outer_tree.nearest(shapely.Point(point)))
        first, second = outer_edges[edge]
        delta = original[second, :2] - original[first, :2]
        parameter = float((point - original[first, :2]) @ delta / (delta @ delta))
        obligations.append(
            {
                "terrain_node": node,
                "body_node": int(row["body"]),
                "original_edge": [int(first), int(second)],
                "parameter": parameter,
                "previous_xyz": before[node].tolist(),
                "required_xyz": terrain[node].tolist(),
            }
        )
    if not checked:
        raise ValueError("no complete bank/terrain edge has been attached")
    return {
        "complete_shared_contact_edges": checked,
        "retained_original_terrain_vertices": retained,
        "changed_original_terrain_vertices": changed,
        "body_facet_plane_residual_max_m": float(residual.max(initial=0)),
        "outer_obligations": obligations,
    }
