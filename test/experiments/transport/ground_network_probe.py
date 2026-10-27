"""Finite geometry on actual source spans, with original admission kept separate."""

import time

import numpy as np
from branched_ground_terminals import audit_sections
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground_recipe import NETWORK_RECIPE
from outshine_ref.transport.canonical_ports._ground_scope import (
    prepare_ground_field_scope,
)
from outshine_ref.transport.canonical_ports._ground_scope_field import fit_scope_field
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._ground_terminal_geometry import (
    rebuild_terminal_caps,
)
from outshine_ref.transport.canonical_ports._ground_terminal_quality import (
    qualify_terminal_surface,
)
from outshine_ref.transport.canonical_ports._ground_terminals import terminal_values
from outshine_ref.transport.canonical_ports._source import component


def run(bundle, terrain, *, seed, budget=None, method="auto", prior_limit=0.5):
    source, groups, ways, binding, _ = bundle
    budget = PortBudget() if budget is None else budget
    began = time.monotonic()
    index, reason, indexed = prepare_ground_scope_index(
        source,
        groups,
        ways,
        source_binding=source.binding,
        candidate_binding=binding,
        budget=budget,
    )
    if reason:
        raise ValueError(reason)
    _, _, _, admission, scope_work = prepare_ground_field_scope(
        source,
        seed,
        groups,
        ways,
        source_binding=source.binding,
        candidate_binding=binding,
        scope_index=index,
        geometry_recipe=NETWORK_RECIPE,
        budget=budget,
    )
    selected, closure, closure_work = component(
        index.index, seed, index.unknown, budget
    )
    if closure not in ("", "unknown_source_frontier"):
        raise ValueError(closure)
    report = {"source_scope": {"members": selected}}
    prepared, reason = fit_scope_field(
        index.index,
        selected,
        terrain,
        prior_limit,
        method,
        budget,
        report,
        NETWORK_RECIPE,
    )
    if reason:
        raise ValueError(reason)
    terminal_reason = ""
    try:
        mesh, caps, work = rebuild_terminal_caps(prepared, report, index)
    except ValueError as error:
        if str(error) != "terminal_source_identity":
            raise
        terminal_reason = str(error)
        mesh, caps, work = report["surface"], (), {}
    mesh, parts, quality, copied = qualify_terminal_surface(
        prepared[1], mesh, report, prior_limit
    )
    values = terminal_values(prepared, mesh, parts, caps, index, budget)
    q = mesh.vertices[mesh.triangles]
    normal = np.cross(q[:, 1] - q[:, 0], q[:, 2] - q[:, 0])
    if np.any(normal[:, 2] <= 0):
        raise ValueError("independent_ground_network_orientation")
    gradients = -normal[:, :2] / normal[:, 2, None]
    frames = report["source_grade_frames"][report["surface_source_faces"]]
    maximum = float(
        np.linalg.norm(np.einsum("fij,fj->fi", frames, gradients), axis=1).max()
    )
    if maximum > 1.0000001:
        raise ValueError("independent_ground_network_grade")
    report.update(
        state="FiniteGeometryDiagnostic",
        source_admission_reason=admission,
        qualified_world_joins=0,
        source_binding=source.binding,
        candidate_binding=binding,
        index_work=indexed,
        scope_work=scope_work,
        closure_work=closure_work,
        original_unknown_count=len(source.unknown_nodes),
        independent_maximum_normalized_finite_grade=maximum,
        terminals={
            "values": values,
            "unavailable_reason": terminal_reason,
            "generated_sections": report["source_terminal_cuts"],
            "whole_quality": quality,
            "independent_sections": audit_sections(prepared[0], mesh, values),
            "work": work,
            "known_owned_plan_payload_copy_bytes": copied,
        },
        elapsed_ms=1000 * (time.monotonic() - began),
        scope="Actual bound spans/DEM and whole geometry controls, not a qualified Source/World join. Original MissingTopology/Unknown and strict scope admission remain unchanged.",
    )
    return report, report["source_input"], prepared[1], mesh
