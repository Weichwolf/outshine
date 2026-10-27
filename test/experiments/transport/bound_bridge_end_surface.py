"""Actual bound Ground terminal to local Bridge end surface, no new authority."""

import time

from bridge_end_surface_geometry import end_geometry
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._end_candidates import inspect_group
from outshine_ref.transport.canonical_ports._ground_recipe import (
    THREE_ARM_RECIPE,
    source_star,
)
from outshine_ref.transport.canonical_ports._ground_terminal_geometry import (
    terminal_plane,
)
from outshine_ref.transport.canonical_ports._source import source_chain


def source_anchor(index, scope):
    members = scope.get("members", ())
    recipe = scope.get("geometry_recipe")
    if recipe is None:
        chain, reason = source_chain(index.index, members)
        if reason:
            raise ValueError(reason)
        return chain[0][0]
    if recipe.get("version") == THREE_ARM_RECIPE:
        star, reason = source_star(index.index, members)
        if reason:
            raise ValueError(reason)
        return index.index[0][star[0]]
    raise ValueError("bridge_end_ground_frame_recipe")


def build_end_surface(prepared, report, index, group_id, *, budget=None):
    started = time.monotonic()
    budget = PortBudget() if budget is None else budget
    binding = report.get("binding") or {}
    scope = binding.get("source_scope", {})
    if (
        report.get("state") != "QualifiedTerminalField"
        or scope.get("source_binding") != index.source.binding
        or scope.get("candidate_binding") != index.candidate_binding
        or report.get("source_scope") != scope
    ):
        raise ValueError("bridge_end_ground_field_binding")
    if not isinstance(prepared, tuple) or len(prepared) != 2:
        raise ValueError("bridge_end_prepared_ground_required")
    if tuple(prepared[0]) != tuple(source_anchor(index, scope)):
        raise ValueError("bridge_end_ground_frame_binding")
    if (
        binding.get("procedure", {}).get("terminal_recipe")
        != "outshine/source-terminal-sections/1"
    ):
        raise ValueError("bridge_end_ground_terminal_recipe")
    values = report.get("terminal_values", ())
    value = next((v for v in values if v["source_terminal"]["group"] == group_id), None)
    if value is None:
        raise ValueError("bridge_end_missing_qualified_ground_terminal")
    cap = value["source_terminal"]
    group = index.groups_by_node.get(cap["source_node"])
    if group is None or group.identifier != group_id:
        raise ValueError("bridge_end_original_group_binding")
    work = {"member_visits": 0}
    candidate = inspect_group(group, index.ways_by_id, index.index, work)
    if (
        candidate["reason"] != "missing_ground_profile"
        or len(candidate["selected"]) != 2
    ):
        raise ValueError(candidate["reason"] or "bridge_ground_pair_required")
    bridge_id, ground_id = candidate["selected"]
    ground_member = next(m for m in group.members if m.edge[:2] == ground_id[0])
    if (
        ground_member.edge != cap["ground_edge"]
        or ground_member.end != cap["ground_end"]
    ):
        raise ValueError("bridge_end_qualified_member_mismatch")
    if (
        budget.requests < 1
        or budget.surface.time_limit_s == 0
        or budget.surface.geometry_time_limit_s == 0
    ):
        raise ValueError("bridge_end_work_budget")
    span = index.index[1][bridge_id[0]]
    node = span.nodes[1 - bridge_id[1]]
    other = index.index[0][node]
    anchor, field = prepared
    gradient, intercept = terminal_plane(field, cap["part"])
    near = report["surface"].vertices[list(cap["cap_vertices"])]
    mesh, quality = end_geometry(
        anchor, group.position, near, gradient, intercept, other, span.width_m, budget
    )
    longs = quality["longitudinal_grade_by_axis_segment"]
    crosses = quality["cross_grade_by_axis_segment"]
    normalized = ((longs / span.maximum_gradient) ** 2 + (crosses / 0.025) ** 2) ** 0.5
    quality["maximum_normalized_source_grade"] = float(normalized.max())
    if (
        float(abs(longs).max()) > span.maximum_gradient
        or float(abs(crosses).max()) > 0.025
        or float(normalized.max()) > 1.0000001
    ):
        raise ValueError("bridge_end_local_profile_grade")
    original = tuple(cap["cap_vertices"][i] for i in quality["near_order"])
    if (
        mesh.vertices[:2].tobytes()
        != report["surface"].vertices[list(original)].tobytes()
    ):
        raise ValueError("bridge_end_original_cap_changed")
    if time.monotonic() - started >= min(
        budget.surface.time_limit_s, budget.surface.geometry_time_limit_s
    ):
        raise ValueError("bridge_end_time_budget")
    return mesh, {
        "state": "MetricEndSurface",
        "group": group_id,
        "bridge_member": bridge_id,
        "ground_member": ground_id,
        "near_ground_vertices": original,
        "source_binding": index.source.binding,
        "candidate_binding": index.candidate_binding,
        "ground_binding": report["binding"],
        "source_frame_anchor": anchor,
        "port_frame_anchor": group.position,
        "quality": quality,
        "work": {
            **work,
            "ground_terminal_rows": len(values),
            "frame_source_spans": len(scope["members"]),
            "additional_DEM_samples": 0,
        },
        "qualified_contacts": 0,
        "qualified_world_joins": 0,
        "pending": (
            "body_cross_section",
            "terrain_and_crossing_clearance",
            "opposite_ground",
            "whole_bridge",
        ),
    }
