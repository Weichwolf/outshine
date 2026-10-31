"""Share newly constructed cut-edge positions with a fixed bank's body mesh.

Finite Double associations use a stated numerical tolerance, no coordinate weld.
Original body vertices survive; both incident faces receive each edge subdivision.
"""

from itertools import pairwise

import numpy as np
import shapely
from shore_bank_strip import contact_edges


def attach(body, terrain, terrain_faces, outer_boundary, *, tolerance_m=1e-8):
    vertices, faces = body["vertices"], body["faces"]
    edges = contact_edges(body)
    a, b = vertices[edges[:, 0]], vertices[edges[:, 1]]
    delta = b[:, :2] - a[:, :2]
    length_squared = np.einsum("ij,ij->i", delta, delta)
    all_edges, counts = np.unique(
        np.sort(terrain_faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1),
        axis=0,
        return_counts=True,
    )
    boundary = np.unique(all_edges[counts == 1])
    if len(boundary) * len(edges) > 1048576 or len(faces) > 8192:
        raise ValueError("bounded contact-edge association required")
    output = terrain.copy()
    additions, split, matches = [], {}, []
    max_xy_error, max_height_delta = 0.0, 0.0
    for node in boundary:
        point = terrain[node, :2]
        t = np.clip(np.sum((point - a[:, :2]) * delta, axis=1) / length_squared, 0, 1)
        feet = a[:, :2] + t[:, None] * delta
        distance = np.linalg.norm(feet - point, axis=1)
        candidates = np.flatnonzero(distance <= tolerance_m)
        if not len(candidates):
            if outer_boundary.distance(shapely.Point(point)) > tolerance_m:
                raise ValueError(
                    "unresolved interior terrain-cut edge; bank ends need continuation"
                )
            continue
        points = a[candidates] + t[candidates, None] * (b[candidates] - a[candidates])
        if np.ptp(points[:, 2]) > tolerance_m:
            raise ValueError("ambiguous fixed body contact height")
        edge = int(candidates[0])
        first, second = map(int, edges[edge])
        parameter = float(t[edge])
        if np.array_equal(point, a[edge, :2]):
            body_node, xyz = first, a[edge].copy()
        elif np.array_equal(point, b[edge, :2]):
            body_node, xyz = second, b[edge].copy()
        else:
            if not 0 < parameter < 1:
                raise ValueError("cut node extends beyond its original body edge")
            xyz = np.r_[point, points[0, 2]]
            body_node = len(vertices) + len(additions)
            additions.append(xyz)
            key = tuple(sorted((first, second)))
            ordered_parameter = parameter if first < second else 1 - parameter
            split.setdefault(key, []).append((ordered_parameter, body_node))
        max_xy_error = max(max_xy_error, float(distance[edge]))
        max_height_delta = max(max_height_delta, abs(float(terrain[node, 2] - xyz[2])))
        output[node] = xyz
        matches.append((int(node), body_node, edge, parameter))
    refined, parents = [], []
    for parent, original in enumerate(faces):
        children = [tuple(map(int, original))]
        for first, second in zip(original, np.roll(original, -1), strict=True):
            key = tuple(sorted((int(first), int(second))))
            if key not in split:
                continue
            nodes = [i for _, i in sorted(split[key], reverse=first > second)]
            owner = next(
                i for i, f in enumerate(children) if first in f and second in f
            )
            opposite = next(i for i in children[owner] if i not in (first, second))
            chain = [int(first), *nodes, int(second)]
            children[owner : owner + 1] = [(u, v, opposite) for u, v in pairwise(chain)]
        refined.extend(children)
        parents.extend([parent] * len(children))
    refined = np.array(refined, np.int64)
    result = np.vstack((vertices, additions)) if additions else vertices.copy()
    if len(refined) > 32768:
        raise ValueError("refined body face budget exhausted")
    triangles = result[refined]
    normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    if np.any(np.linalg.norm(normal, axis=1) == 0):
        raise ValueError("body subdivision produced a collapsed facet")
    oriented = refined[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    unique, counts = np.unique(np.sort(oriented, axis=1), axis=0, return_counts=True)
    if np.any(counts != 2) or len(np.unique(oriented, axis=0)) != 2 * len(unique):
        raise ValueError("body edge subdivision is not a closed oriented mesh")
    # Record exact output identity, distinct from proximity to the source facet.
    for node, body_node, _, _ in matches:
        if output[node].tobytes() != result[body_node].tobytes():
            raise ValueError("terrain and body contact position words differ")
    return (
        output,
        result,
        refined,
        np.array(
            matches,
            dtype=[
                ("terrain", "<u4"),
                ("body", "<u4"),
                ("edge", "<u4"),
                ("parameter", "<f8"),
            ],
        ),
        np.array(parents, dtype="<u4"),
        {
            "matched_contact_nodes": len(matches),
            "body_added_vertices": len(additions),
            "body_added_faces": len(refined) - len(faces),
            "association_distance_max_m": max_xy_error,
            "contact_height_change_max_m": max_height_delta,
        },
    )
