"""Independent own-frame diagnostics on the shared finite terminal construction."""

import numpy as np
from outshine_ref.transport.canonical_ports._ground_terminal_geometry import (
    rebuild_terminal_caps,
)
from outshine_ref.transport.canonical_ports._ground_terminal_quality import (
    qualify_terminal_surface,
)
from outshine_ref.transport.canonical_ports._ground_terminals import terminal_values


def construct(anchor, field, inputs, original_caps, index, budget, *, prior_limit):
    report = {
        "source_input": inputs,
        "surface": field.plan.mesh,
        "surface_source_faces": field.plan.source_faces,
        "source_terminal_cuts": original_caps,
        "source_scope": {"members": tuple(cap["ground_edge"] for cap in original_caps)},
    }
    mesh, caps, work = rebuild_terminal_caps((anchor, field), report, index)
    mesh, parts, quality, copied = qualify_terminal_surface(
        field, mesh, report, prior_limit
    )
    values = terminal_values((anchor, field), mesh, parts, caps, index, budget)
    assert len(values) == 3
    return mesh, {
        **work,
        "scope": "Finite diagnostic on shared helpers; no canonical variant/World binding.",
        "values": values,
        "whole_quality": quality,
        "known_owned_plan_payload_copy_bytes": copied,
        "independent_sections": audit_sections(anchor, mesh, values),
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
