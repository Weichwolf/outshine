"""Actual bound terminal values and finite sections, without a World-node join."""

import math
import time
from dataclasses import replace

import numpy as np

from ...contracts import immutable
from ._ground_scope_field import derive_scoped_ground_field
from ._ground_scope_index import prepare_ground_scope_index
from ._ground_terminal_geometry import rebuild_terminal_caps
from ._ground_terminal_quality import qualify_terminal_surface
from ._queries import CoarseDomain, canonical_port
from ._types import PortBudget


def terminal_values(prepared, mesh, parts, caps, scope_index, budget):
    anchor, field = prepared
    field.domain = CoarseDomain(
        mesh, parts, max_queries=min(4096, budget.surface.max_queries)
    )
    values = []
    for cap in caps:
        value = canonical_port(
            field,
            anchor,
            cap["anchor_longitude_latitude"],
            cap["source_segment"],
            cap["half_width_m"],
            cap["initial_model_height_m"],
        )
        span = scope_index.index[1][cap["ground_edge"][:2]]
        if abs(value["longitudinal_grade"]) > span.maximum_gradient + 1e-9:
            raise ValueError("physical_port_longitudinal_grade_limit")
        if abs(math.tan(value["crossfall_rad"])) > 0.025 + 1e-9:
            raise ValueError("physical_port_cross_grade_limit")
        values.append(immutable({"source_terminal": cap, "port": value}))
    return tuple(values)


def derive_scoped_ground_terminals(
    source,
    seed,
    groups,
    ways,
    terrain,
    *,
    source_binding,
    candidate_binding,
    terrain_binding,
    maximum_prior_change_m,
    budget=None,
    method="auto",
    scope_index=None,
):
    """Fit the existing field and construct both actual source terminal sections.

    Terminal heights come from intersecting the fitted end planes with their bound
    geodetic source cuts. The existing bounded query then checks the constructed
    finite domain; no arbitrary outside evaluation becomes a physical contact.
    """
    budget = PortBudget() if budget is None else budget
    if not isinstance(budget, PortBudget) or not callable(terrain):
        raise TypeError("existing budget and bound terrain sampler required")
    started = time.monotonic()
    report = {
        "state": "Pending",
        "terrain_samples": 0,
        "binding": None,
        "source_scope": None,
        "field": None,
        "terminal_values": (),
        "qualified_world_joins": 0,
        "index_build_work": {},
    }
    if budget.requests < 2 or budget.surface.max_queries < 2:
        return None, "ground_terminal_query_budget", report
    if budget.surface.geometry_time_limit_s == 0 or budget.surface.time_limit_s == 0:
        return None, "ground_terminal_time_budget", report
    if scope_index is None:
        scope_index, reason, work = prepare_ground_scope_index(
            source,
            groups,
            ways,
            source_binding=source_binding,
            candidate_binding=candidate_binding,
            budget=budget,
        )
        report["index_build_work"] = work
        if reason:
            return None, reason, report
    remaining = budget.surface.time_limit_s - (time.monotonic() - started)
    if remaining <= 0:
        return None, "ground_terminal_time_budget", report
    local = replace(
        budget,
        surface=replace(
            budget.surface,
            time_limit_s=remaining,
            solver_time_limit_s=min(budget.surface.solver_time_limit_s, remaining),
            geometry_time_limit_s=min(budget.surface.geometry_time_limit_s, remaining),
        ),
    )
    prepared, reason, fitted = derive_scoped_ground_field(
        source,
        seed,
        groups,
        ways,
        terrain,
        source_binding=source_binding,
        candidate_binding=candidate_binding,
        terrain_binding=terrain_binding,
        maximum_prior_change_m=maximum_prior_change_m,
        budget=local,
        method=method,
        scope_index=scope_index,
    )
    fitted["index_build_work"] = report["index_build_work"]
    report = fitted
    report["state"] = "Pending"
    if reason:
        return None, reason, report
    geometry_started = time.monotonic()
    deadline = min(
        started + budget.surface.time_limit_s,
        geometry_started + budget.surface.geometry_time_limit_s,
    )
    if time.monotonic() >= deadline:
        return None, "ground_terminal_time_budget", report
    try:
        mesh, caps, work = rebuild_terminal_caps(prepared, report, scope_index)
        report["terminal_work"] = work
        if time.monotonic() >= deadline:
            raise ValueError("ground_terminal_time_budget")
        mesh, parts, checked, copied_payload = qualify_terminal_surface(
            prepared[1], mesh, report, maximum_prior_change_m
        )
        work["owned_plan_payload_copy_bytes"] = copied_payload
        work["copy_scope"] = (
            "Known construction and immutable-plan payload copies only; temporary numeric gathers, query storage and total bytes are not instrumented"
        )
        if time.monotonic() >= deadline:
            raise ValueError("ground_terminal_time_budget")
        values = terminal_values(prepared, mesh, parts, caps, scope_index, budget)
    except (ValueError, OSError, MemoryError, np.linalg.LinAlgError) as error:
        return None, "ground_terminal_unavailable:" + str(error), report
    if time.monotonic() >= deadline:
        return None, "ground_terminal_time_budget", report
    base = report["binding"]
    binding = immutable(
        {
            **base,
            "procedure": {
                **base["procedure"],
                "terminal_recipe": "outshine/source-terminal-sections/1",
            },
            "terminal_sections": tuple(
                {
                    k: v
                    for k, v in cap.items()
                    if k
                    not in (
                        "initial_model_height_m",
                        "plane_source_gradient",
                        "plane_source_intercept",
                    )
                }
                for cap in caps
            ),
        }
    )
    report.update(
        state="QualifiedTerminalField",
        binding=binding,
        surface=mesh,
        terminal_values=values,
        finite_quality=checked,
        terminal_quality=checked,
        terminal_work=work,
    )
    work["port_queries"] = prepared[1].queries
    work["face_bounds_tests"] = prepared[1].face_bounds_tests
    work["geometry_and_queries_seconds"] = time.monotonic() - geometry_started
    return prepared, "", report
