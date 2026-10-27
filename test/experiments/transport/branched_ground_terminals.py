"""Private three-cap audit using the existing finite construction and query helpers."""

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport.canonical_ports._ground_terminal_geometry import (
    boundary_edges,
    cap_path,
    intersect_section,
    section_geometry,
    terminal_plane,
)
from outshine_ref.transport.canonical_ports._ground_terminal_quality import (
    qualify_terminal_surface,
)
from outshine_ref.transport.canonical_ports._ground_terminals import terminal_values


def construct(anchor, field, inputs, original_caps, index, budget, *, prior_limit):
    old = field.plan.mesh
    vertices = old.vertices.copy()
    boundary = boundary_edges(old)
    caps = []
    for original in original_caps:
        node = original["source_node"]
        ends = original["original_vertices"]
        group = index.groups_by_node[node]
        members = [
            m
            for m in group.members
            if m.domain_node == node and m.edge[:2] == tuple(original["ground_edge"])
        ]
        assert len(members) == 1 and members[0].source_terminal is True
        member = members[0]
        span = index.index[1][member.edge[:2]]
        way = index.ways_by_id[member.source_way]
        segment = way.first_segment if member.physical_end == 0 else way.last_segment
        faces = np.flatnonzero(
            np.count_nonzero(np.isin(inputs.surface.triangles, ends), axis=1) == 2
        )
        assert len(faces) == 1
        part = int(inputs.face_parts[faces[0]])
        assert inputs.part_roles[part] == 1
        path = cap_path(
            boundary,
            field.plan.source_faces,
            int(faces[0]),
            field.plan.original_vertex_map[list(ends)],
        )
        gradient, intercept = terminal_plane(field, part)
        at, left, up = section_geometry(anchor, group.position, segment)
        current, initial = intersect_section(
            vertices[path], span.width_m / 2, at, left, up, gradient, intercept
        )
        vertices[path] = current
        caps.append(
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
    mesh, parts, quality, copied = qualify_terminal_surface(
        field, Mesh(vertices, old.triangles), {"source_input": inputs}, prior_limit
    )
    values = terminal_values((anchor, field), mesh, parts, caps, index, budget)
    assert len(values) == 3
    independent = audit_sections(anchor, mesh, values)
    return mesh, {
        "scope": "Private three-cap finite/numeric control on source-checked inputs. No new accepted source scope, public producer, bridge or World authority.",
        "values": values,
        "whole_quality": quality,
        "known_owned_plan_payload_copy_bytes": copied,
        "triangle_edge_visits": 3 * len(old.triangles),
        "boundary_edge_scan_visits": len(boundary) * len(caps),
        "edge_work_scope": "Enumeration and full cap-path scans only; BFS and quality work excluded.",
        "independent_sections": independent,
        "maximum_vertex_shift_m": float(
            np.linalg.norm(vertices - old.vertices, axis=1).max()
        ),
    }


def audit_sections(anchor, mesh, values):
    independent = []
    from outshine_ref.transport.canonical_ports._geodesy import frame

    origin, axes = frame(anchor)
    for item in values:
        cap, port = item["source_terminal"], item["port"]
        target, target_axes = frame(cap["anchor_longitude_latitude"])
        at, basis = axes.T @ (target - origin), axes.T @ target_axes
        own = (mesh.vertices[list(cap["cap_vertices"])] - at) @ basis
        direction = np.array(port["direction_EN"])
        left = np.array([-direction[1], direction[0]])
        longitudinal = own[:, :2] @ direction
        transverse = own[:, :2] @ left
        assert abs(longitudinal).max() < 1e-7
        np.testing.assert_allclose(
            np.sort(transverse)[[0, -1]],
            [-cap["half_width_m"], cap["half_width_m"]],
            rtol=0,
            atol=1e-7,
        )
        independent.append(
            {
                "group": cap["group"],
                "own_longitudinal_error_m": float(abs(longitudinal).max()),
                "own_width_m": float(np.ptp(transverse)),
                "height_m": port["height_m"],
                "longitudinal_grade": port["longitudinal_grade"],
                "crossfall_rad": port["crossfall_rad"],
            }
        )
    return independent
