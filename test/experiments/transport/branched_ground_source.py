"""Source-checked diagnostics using the shared scope and geometry procedures."""

import time

import numpy as np
from outshine_ref.transport.canonical_ports._geodesy import positions_in_frame
from outshine_ref.transport.canonical_ports._ground_recipe import (
    THREE_ARM_RECIPE,
    source_star,
)
from outshine_ref.transport.canonical_ports._ground_scope import (
    prepare_ground_field_scope,
)
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._ground_star_geometry import layout
from outshine_ref.transport.canonical_ports._ground_star_input import (
    lift_star_input,
    prepare_star_ground,
)


def checked_star(source, seed, groups, ways, binding, budget):
    index, reason, index_work = prepare_ground_scope_index(
        source,
        groups,
        ways,
        source_binding=source.binding,
        candidate_binding=binding,
        budget=budget,
    )
    if reason:
        raise ValueError(reason)
    _, edges, scope, reason, work = prepare_ground_field_scope(
        source,
        seed,
        groups,
        ways,
        source_binding=source.binding,
        candidate_binding=binding,
        budget=budget,
        scope_index=index,
        geometry_recipe=THREE_ARM_RECIPE,
    )
    if reason:
        raise ValueError(reason)
    star, reason = source_star(index.index, edges)
    if reason:
        raise ValueError(reason)
    centre, leaves = star
    return (
        index,
        edges,
        centre,
        leaves,
        {
            "state": "QualifiedSourceScope",
            "index_work": index_work,
            "source_work": work,
            "typed_exits": scope["typed_exits"],
            "original_unknown_count": len(source.unknown_nodes),
            "scope": "Shared source preflight only; diagnostic geometry variants do not gain a canonical field/World binding.",
        },
    )


def source_input(
    source,
    index,
    edges,
    centre,
    leaves,
    terrain,
    budget,
    *,
    proximal=True,
    cut_method="linear",
    deadline=None,
):
    if index.source is not source:
        raise ValueError("diagnostic_ground_index_source_mismatch")
    if proximal and cut_method == "linear":
        return prepare_star_ground(
            index.index, edges, terrain, budget, deadline=deadline
        )
    # Noncanonical comparison paths retain the same source preflight and shared lift.
    # They cannot be requested through the common geometry_recipe entry.
    started = time.monotonic()
    deadline = started + budget.surface.time_limit_s if deadline is None else deadline
    geometry_deadline = min(deadline, started + budget.surface.geometry_time_limit_s)
    nodes, spans, _ = index.index
    anchor = nodes[centre]
    ends = positions_in_frame(anchor, [nodes[n] for n in leaves], np.zeros(3))[:, :2]
    widths, grades = (
        [spans[e].width_m for e in edges],
        [spans[e].maximum_gradient for e in edges],
    )
    raw, geometry = layout(
        ends,
        widths,
        grades,
        proximal=proximal,
        cut_method=cut_method,
        seconds=geometry_deadline - time.monotonic(),
    )
    geometry.update(
        source_anchor=anchor,
        source_spans=edges,
        source_end_nodes=leaves,
        widths_m=widths,
        maximum_gradients=grades,
    )
    return lift_star_input(
        anchor,
        raw,
        geometry,
        terrain,
        budget,
        edges=edges,
        leaves=leaves,
        deadline=deadline,
        geometry_deadline=geometry_deadline,
    )
