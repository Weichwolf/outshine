"""Finite strip end sections from bound source cuts and fitted terminal planes."""

from collections import deque

import numpy as np

from ...contracts import Mesh
from ._geodesy import frame, positions_in_frame
from ._source import source_chain


def boundary_edges(mesh):
    edges = {}
    for face, corners in enumerate(mesh.triangles):
        for i, j in ((0, 1), (1, 2), (2, 0)):
            key = tuple(sorted((int(corners[i]), int(corners[j]))))
            count, owner = edges.get(key, (0, face))
            if count == 2:
                raise ValueError("terminal_nonmanifold_surface")
            edges[key] = count + 1, owner
    return tuple(
        (a, b, owner) for (a, b), (count, owner) in edges.items() if count == 1
    )


def cap_path(boundary, parents, source_face, anchors):
    adjacency = {}
    for a, b, face in boundary:
        if parents[face] == source_face:
            adjacency.setdefault(a, []).append(b)
            adjacency.setdefault(b, []).append(a)
    start, end = map(int, anchors)
    queue, previous = deque([start]), {start: None}
    while queue:
        node = queue.popleft()
        if len(adjacency.get(node, ())) > 2:
            raise ValueError("terminal_boundary_branch")
        for adjacent in adjacency.get(node, ()):
            if adjacent not in previous:
                previous[adjacent] = node
                queue.append(adjacent)
            elif previous[node] != adjacent:
                raise ValueError("terminal_boundary_cycle")
    if end not in previous:
        raise ValueError("terminal_boundary_missing")
    path = [end]
    while path[-1] != start:
        path.append(previous[path[-1]])
    return np.asarray(path[::-1], dtype=np.int64)


def terminal_plane(field, part):
    if not hasattr(field, "plan"):
        return field.gradient, float(field.intercept - field.gradient @ field.origin)
    plan = field.plan
    parents = plan.field.plane_ids[part]
    if parents[0] != parents[1]:
        raise ValueError("terminal_patch_not_plane")
    coefficients = plan.plane_coefficients.reshape(-1, 3)[parents[0]]
    gradient = coefficients[1:]
    return gradient, float(
        coefficients[0] + plan.datum_m - gradient @ plan.field.origins[parents[0]]
    )


def section_geometry(anchor, position, segment):
    origin, axes = frame(anchor)
    target, target_axes = frame(position)
    at, basis = axes.T @ (target - origin), axes.T @ target_axes
    ends = positions_in_frame(position, np.asarray(segment), np.zeros(2))
    direction = ends[1, :2] - ends[0, :2]
    length = float(np.linalg.norm(direction))
    if not np.isfinite(length) or length <= 1e-9:
        raise ValueError("terminal_source_direction")
    direction /= length
    return at, basis[:, :2] @ np.array([-direction[1], direction[0]]), basis[:, 2]


def intersect_section(old, half_width, at, left, up, gradient, intercept):
    distance = np.r_[0.0, np.cumsum(np.linalg.norm(np.diff(old, axis=0), axis=1))]
    if distance[-1] <= 0:
        raise ValueError("terminal_collapsed_section")
    sides = np.sign((old[[0, -1]] - at) @ left)
    if sides[0] * sides[1] != -1:
        raise ValueError("terminal_source_side_binding")
    offsets = half_width * (sides[0] + distance / distance[-1] * (sides[1] - sides[0]))
    normal = np.r_[-gradient, 1.0]
    derivative = float(normal @ up)
    if not np.isfinite(derivative) or derivative <= 0.25:
        raise ValueError("terminal_surface_not_upward")
    base = at + offsets[:, None] * left
    heights = (intercept - base @ normal) / derivative
    vertices = base + heights[:, None] * up
    centre_height = float((intercept - at @ normal) / derivative)
    return vertices, centre_height


def terminal_specs(report, scope_index):
    if "source_terminal_cuts" in report:
        return tuple(
            (cap["original_vertices"], cap["source_node"])
            for cap in report["source_terminal_cuts"]
        )
    inputs = report["source_input"]
    chain, reason = source_chain(scope_index.index, report["source_scope"]["members"])
    if reason:
        raise ValueError(reason)
    return (
        ((0, 1), chain[1][0]),
        (
            (len(inputs.surface.vertices) - 2, len(inputs.surface.vertices) - 1),
            chain[1][-1],
        ),
    )


def rebuild_terminal_caps(prepared, report, scope_index):
    anchor, field = prepared
    inputs, mesh = report["source_input"], report["surface"]
    selected = report["source_scope"]["members"]
    vertices, boundary = mesh.vertices.copy(), boundary_edges(mesh)
    parents = report["surface_source_faces"]
    vertex_map = (
        field.plan.original_vertex_map
        if hasattr(field, "plan")
        else np.arange(len(vertices))
    )
    descriptors, maximum_shift = [], 0.0
    for original, node in terminal_specs(report, scope_index):
        group = scope_index.groups_by_node[node]
        members = [
            m for m in group.members if m.domain_node == node and m.edge[:2] in selected
        ]
        if len(members) != 1 or members[0].source_terminal is not True:
            raise ValueError("terminal_source_identity")
        member = members[0]
        span = scope_index.index[1][member.edge[:2]]
        way = scope_index.ways_by_id[member.source_way]
        segment = way.first_segment if member.physical_end == 0 else way.last_segment
        faces = np.flatnonzero(
            np.count_nonzero(np.isin(inputs.surface.triangles, original), axis=1) == 2
        )
        if len(faces) != 1 or inputs.part_roles[inputs.face_parts[faces[0]]] != 1:
            raise ValueError("terminal_source_cap_face")
        part = int(inputs.face_parts[faces[0]])
        path = cap_path(boundary, parents, int(faces[0]), vertex_map[list(original)])
        gradient, intercept = terminal_plane(field, part)
        at, left, up = section_geometry(anchor, group.position, segment)
        old = vertices[path]
        current, initial = intersect_section(
            old, span.width_m / 2, at, left, up, gradient, intercept
        )
        vertices[path] = current
        maximum_shift = max(
            maximum_shift, float(np.linalg.norm(current - old, axis=1).max())
        )
        descriptors.append(
            {
                "group": group.identifier,
                "source_node": node,
                "ground_edge": member.edge,
                "ground_end": member.end,
                "source_way": member.source_way,
                "source_segment": segment,
                "anchor_longitude_latitude": group.position,
                "half_width_m": span.width_m / 2,
                "cap_vertices": tuple(map(int, path)),
                "part": part,
                "plane_source_gradient": tuple(map(float, gradient)),
                "plane_source_intercept": intercept,
                "initial_model_height_m": initial,
            }
        )
    vertices.setflags(write=False)
    return (
        Mesh(vertices, mesh.triangles),
        tuple(descriptors),
        {
            "triangle_edge_visits": 3 * len(mesh.triangles),
            "boundary_edge_scan_visits": len(boundary) * len(descriptors),
            "edge_work_scope": "Triangle-edge enumeration and full cap-path boundary scans only; BFS, numerical quality and coverage work are not counted here. Queries have separate counters.",
            "construction_payload_copy_bytes": 2 * vertices.nbytes
            + mesh.triangles.nbytes
            + 24 * sum(len(d["cap_vertices"]) for d in descriptors),
            "cap_vertices": sum(len(d["cap_vertices"]) for d in descriptors),
            "maximum_vertex_shift_m": maximum_shift,
        },
    )
