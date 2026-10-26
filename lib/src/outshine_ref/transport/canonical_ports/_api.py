"""Source closure → bound DEM observations → shared field → canonical port values."""

import math
import time
from collections.abc import Mapping
from dataclasses import replace

import numpy as np

from ._ground import fit_ground, prepare_ground
from ._queries import canonical_port
from ._source import component, ground_chain, source_index, validate_cut
from ._types import PortBudget, PortDemand, PortResult, PortSource, PortValue

DEFAULT_BUDGET = PortBudget()


def result(values, reasons, source, terrain_binding, components, work, settings):
    return PortResult(
        tuple(values),
        tuple(reasons),
        {
            "source_binding": source.binding,
            "terrain_binding": terrain_binding,
            "components": components,
            "work": work,
            "procedure": settings,
            "scope": (
                "Numeric ports in exhausted admitted normalized source components. "
                "No whole-world, native plan/cache, physical simulation or device admission. "
                "Bridge/tunnel, branching and mixed-profile ground recipes remain explicit Pending."
            ),
        },
    )


def component_field(index, selected, terrain, error_limit_m, method, budget):
    chain, reason = ground_chain(index, selected)
    if reason:
        return None, reason, {}
    try:
        anchor, inputs, samples = prepare_ground(chain, terrain, budget)
        field, report = fit_ground(inputs, error_limit_m, method, budget)
    except (ValueError, OSError, np.linalg.LinAlgError) as error:
        return None, "ground_field_unavailable:" + str(error), {}
    report.update(
        terrain_samples=samples,
        input_vertices=len(inputs.surface.vertices),
        input_faces=len(inputs.surface.triangles),
        input_payload_bytes=sum(
            array.nbytes
            for array in (
                inputs.surface.vertices,
                inputs.surface.triangles,
                inputs.face_parts,
                inputs.part_roles,
                inputs.port_edges,
                inputs.port_parts,
                inputs.grade_frames,
            )
        ),
    )
    if field is None:
        return (
            None,
            "ground_field_unavailable:" + report.get("reason", "unresolved"),
            report,
        )
    return (anchor, field), "", report


def port_value(demand, span, nodes, prepared, terrain):
    anchor, field = prepared
    geographic = np.array([nodes[node] for node in span.nodes])
    position = np.array(demand.anchor_longitude_latitude)
    sampled = np.asarray(terrain(position[None, :]), float)
    if sampled.shape != (1,) or not np.isfinite(sampled).all():
        raise ValueError("missing_or_invalid_port_DEM_stencil")
    numeric = canonical_port(
        field, anchor, position, geographic, span.width_m / 2, float(sampled[0])
    )
    if abs(numeric["longitudinal_grade"]) > span.maximum_gradient + 1e-9:
        raise ValueError("physical_port_longitudinal_grade_limit")
    if abs(math.tan(numeric["crossfall_rad"])) > 0.025 + 1e-9:
        raise ValueError("physical_port_cross_grade_limit")
    return PortValue(
        demand.edge,
        demand.boundary,
        demand.anchor_longitude_latitude,
        numeric["height_m"],
        tuple(numeric["direction_EN"]),
        tuple(numeric["plane_EN"]),
        numeric["half_width_m"],
        numeric["longitudinal_grade"],
        numeric["crossfall_rad"],
        span.attributes,
    )


def derive_canonical_ports(
    source,
    demands,
    terrain,
    *,
    terrain_binding,
    maximum_prior_change_m,
    budget=DEFAULT_BUDGET,
    method="auto",
):
    """Derive numeric values from normalized source geometry and a bound sampler.

    The sampler accepts longitude/latitude rows and returns explicitly mapped model
    geodetic heights. It is called only after finite closure/profile admission.
    Metadata identifies the actual source datum and the model-height mapping; no
    geoid conversion or measured ellipsoid equivalence is inferred. The caller's
    explicit prior-change limit is a generator quality input, not a world gate.
    Times are cooperative offline limits; callbacks/solvers require bounded workers.
    """
    if not isinstance(source, PortSource) or not isinstance(budget, PortBudget):
        raise TypeError("bound PortSource and PortBudget required")
    if not isinstance(demands, tuple) or any(
        not isinstance(d, PortDemand) for d in demands
    ):
        raise TypeError("immutable canonical source-cut demands required")
    if (
        not math.isfinite(maximum_prior_change_m)
        or not 0 < maximum_prior_change_m <= 1000
    ):
        raise ValueError(
            "explicit bounded positive ground-prior quality limit required"
        )
    if method not in ("auto", "affine", "cone", "lp") or not callable(terrain):
        raise ValueError("declared field method and bound terrain sampler required")
    if (
        not isinstance(terrain_binding, Mapping)
        or not isinstance(terrain_binding.get("height_mapping"), str)
        or not terrain_binding["height_mapping"]
        or not isinstance(terrain_binding.get("source_datum"), str)
        or not terrain_binding["source_datum"]
        or not terrain_binding.get("sources")
    ):
        raise ValueError("actual terrain source and height-mapping binding required")
    started = time.monotonic()
    values, reasons = [None] * len(demands), [""] * len(demands)
    reports, work = [], {"terrain_port_samples": 0, "terrain_samples": 0}
    settings = {
        "version": 2,
        "requested_method": method,
        "maximum_prior_change_m": maximum_prior_change_m,
        "requires_terminal_solver": True,
    }

    def sample(positions):
        count = len(positions)
        if count > budget.terrain_samples - work["terrain_samples"]:
            raise ValueError("terrain_sample_budget")
        if time.monotonic() - started >= budget.surface.time_limit_s:
            raise ValueError("terrain_sampling_time_budget")
        work["terrain_samples"] += count
        return terrain(positions)

    if len(demands) > budget.requests or budget.surface.time_limit_s == 0:
        reasons[:] = ["request_or_time_budget"] * len(demands)
        return result(values, reasons, source, terrain_binding, reports, work, settings)
    index, reason = source_index(source, budget)
    if reason:
        reasons[:] = [reason] * len(demands)
        return result(values, reasons, source, terrain_binding, reports, work, settings)
    work.update(validated_nodes=len(source.nodes), validated_spans=len(source.spans))
    nodes, spans, _incidence = index
    for i, demand in enumerate(demands):
        if demand.edge not in spans:
            reasons[i] = "missing_requested_source"
        else:
            reasons[i] = validate_cut(demand, spans[demand.edge], nodes)
    unknown, resolved = set(source.unknown_nodes), {}
    for identity in sorted({d.edge for i, d in enumerate(demands) if not reasons[i]}):
        if identity in resolved:
            continue
        selected, reason, counted = component(index, identity, unknown, budget)
        report = {
            "selected": selected,
            "closure_reason": reason,
            "closure_work": counted,
        }
        prepared = None
        if not reason:
            if time.monotonic() - started >= budget.surface.time_limit_s:
                reason = "port_preparation_time_budget"
            else:
                remaining = max(
                    0.0, budget.surface.time_limit_s - (time.monotonic() - started)
                )
                local_budget = replace(
                    budget,
                    surface=replace(
                        budget.surface,
                        time_limit_s=remaining,
                        solver_time_limit_s=min(
                            budget.surface.solver_time_limit_s, remaining
                        ),
                        geometry_time_limit_s=min(
                            budget.surface.geometry_time_limit_s, remaining
                        ),
                    ),
                )
                prepared, reason, fitting = component_field(
                    index,
                    selected,
                    sample,
                    maximum_prior_change_m,
                    method,
                    local_budget,
                )
                report["field"] = fitting
        report["reason"] = reason
        reports.append(report)
        for edge in selected or (identity,):
            resolved[edge] = prepared, reason
    ordered = sorted(
        range(len(demands)),
        key=lambda i: (
            demands[i].edge,
            demands[i].boundary,
            demands[i].fraction,
            demands[i].anchor_longitude_latitude,
        ),
    )
    for i in ordered:
        if reasons[i]:
            continue
        demand = demands[i]
        prepared, reason = resolved[demand.edge]
        if not reason:
            reason = validate_cut(demand, spans[demand.edge], nodes)
        if not reason and time.monotonic() - started >= budget.surface.time_limit_s:
            reason = "port_query_time_budget"
        if not reason:
            try:
                work["terrain_port_samples"] += 1
                values[i] = port_value(
                    demand, spans[demand.edge], nodes, prepared, sample
                )
            except (ValueError, OSError, np.linalg.LinAlgError) as error:
                reason = "port_query_unavailable:" + str(error)
        reasons[i] = reason
    for report in reports:
        selected = report["selected"]
        prepared = resolved[selected[0]][0] if selected else None
        if prepared is not None:
            field = prepared[1]
            report["query_work"] = {
                "field_evaluations": field.queries,
                "face_bounds_tests": field.face_bounds_tests,
            }
    return result(values, reasons, source, terrain_binding, reports, work, settings)
