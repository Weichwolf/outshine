"""Existing ground fitter behind the explicit original-fact field request."""

import math
import time
from collections.abc import Mapping
from dataclasses import replace
from types import MappingProxyType

import numpy as np

from ...contracts import Mesh, immutable
from ..port_heights._world import finite_ports, normal_gradients
from ._ground import fit_ground, prepare_ground
from ._ground_scope import prepare_ground_field_scope
from ._source import source_chain
from ._types import PortBudget


def affine_surface(inputs, field):
    vertices = inputs.surface.vertices.copy()
    vertices[:, 2] = field.intercept + (vertices[:, :2] - field.origin) @ field.gradient
    faces = inputs.surface.triangles
    gradients = normal_gradients(vertices[:, :2], faces, vertices[:, 2])
    normalized = np.linalg.norm(
        np.einsum("fij,fj->fi", inputs.grade_frames, gradients), axis=1
    )
    roots = {
        tuple(sorted(map(int, edge))): i for i, edge in enumerate(inputs.port_edges)
    }
    bad, contacts = finite_ports(
        vertices[:, :2],
        faces,
        inputs.face_parts,
        vertices[:, 2],
        roots,
        inputs.part_roles,
    )
    if float(max(normalized)) > 1.0000001 or len(bad):
        raise ValueError("scoped_affine_finite_quality")
    vertices.setflags(write=False)
    parents = np.arange(len(faces), dtype=np.int64)
    parents.setflags(write=False)
    quality = {
        "maximum_normalized_finite_grade": float(max(normalized)),
        "finite_contacts": contacts,
        "height_chord_bound_m": 0.0,
        "scope": "Same affine plane over the unchanged complete finite input topology; existing1.0000001 finite-grade and0.005contact limits",
    }
    return Mesh(vertices, faces), parents, quality


def fit_scope_field(index, selected, sample, limit, method, budget, report):
    chain, reason = source_chain(index, selected)
    if reason:
        return None, reason
    try:
        anchor, inputs, _count = prepare_ground(chain, sample, budget)
        field, fitting = fit_ground(inputs, limit, method, budget)
    except (ValueError, OSError, np.linalg.LinAlgError) as error:
        return None, "ground_field_unavailable:" + str(error)
    report["field"] = fitting
    report["source_prior_surface"] = inputs.surface
    report["source_grade_frames"] = inputs.grade_frames
    report["source_input"] = inputs
    if field is None:
        return None, "ground_field_unavailable:" + fitting.get("reason", "unresolved")
    if hasattr(field, "plan"):
        surface = field.plan.mesh
        parents = field.plan.source_faces
    else:
        try:
            surface, parents, quality = affine_surface(inputs, field)
        except ValueError as error:
            return None, "ground_field_unavailable:" + str(error)
        report["finite_quality"] = quality
    report.update(
        surface=surface, surface_source_faces=parents, finite_mesh_constructed=True
    )
    return (anchor, field), ""


def derive_scoped_ground_field(
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
    """Return (existing prepared field, reason, bound report), with no World join.

    The new scope can qualify geometry despite logical Unknown only through the
    original-member proof. The established strict port producer is unchanged.
    The supplied prior limit remains mandatory; no solver result increases it.
    A scope_index may reuse the same owned source/fact indexes across fields;
    it does not retain fitted fields or alter request/binding/quality semantics.
    """
    budget = PortBudget() if budget is None else budget
    if not isinstance(budget, PortBudget) or not callable(terrain):
        raise TypeError("existing budget and bound terrain sampler required")
    if (
        method not in ("auto", "affine", "cone", "lp")
        or not math.isfinite(maximum_prior_change_m)
        or not 0 < maximum_prior_change_m <= 1000
    ):
        raise ValueError(
            "existing ground method and explicit positive prior limit required"
        )
    if (
        not isinstance(terrain_binding, Mapping)
        or not terrain_binding.get("sources")
        or not isinstance(terrain_binding.get("source_datum"), str)
        or not terrain_binding["source_datum"]
        or not isinstance(terrain_binding.get("height_mapping"), str)
        or not terrain_binding["height_mapping"]
    ):
        raise ValueError("actual bound terrain sources/datum/height mapping required")
    report = {
        "state": "Pending",
        "source_scope": None,
        "binding": None,
        "terrain_samples": 0,
        "field": None,
        "qualified_world_joins": 0,
    }
    if budget.surface.time_limit_s == 0:
        return None, "ground_scope_time_budget", report
    started = time.monotonic()
    index, selected, scope, reason, work = prepare_ground_field_scope(
        source,
        seed,
        groups,
        ways,
        source_binding=source_binding,
        candidate_binding=candidate_binding,
        budget=budget,
        scope_index=scope_index,
    )
    report.update(source_scope=scope, source_work=work)
    if reason:
        return None, reason, report
    binding = MappingProxyType(
        {
            "source_scope": scope,
            "terrain_binding": immutable(terrain_binding),
            "procedure": immutable(
                {
                    "version": "outshine/scoped-canonical-ground-field/1",
                    "existing_field_procedure": 2,
                    "requested_method": method,
                    "maximum_prior_change_m": maximum_prior_change_m,
                    "continuous_cross_gradient": 0.025,
                    "height_chord_m": 0.01,
                    "finite_contact_gradient_jump": 0.005,
                }
            ),
        }
    )
    report["binding"] = binding
    remaining = max(0.0, budget.surface.time_limit_s - (time.monotonic() - started))
    if remaining == 0:
        return None, "ground_scope_time_budget", report
    local_budget = replace(
        budget,
        surface=replace(
            budget.surface,
            time_limit_s=remaining,
            solver_time_limit_s=min(budget.surface.solver_time_limit_s, remaining),
            geometry_time_limit_s=min(budget.surface.geometry_time_limit_s, remaining),
        ),
    )

    def sample(geographic):
        if len(geographic) > budget.terrain_samples - report["terrain_samples"]:
            raise ValueError("terrain_sample_budget")
        if time.monotonic() - started >= budget.surface.time_limit_s:
            raise ValueError("ground_scope_time_budget")
        report["terrain_samples"] += len(geographic)
        return terrain(geographic)

    prepared, reason = fit_scope_field(
        index, selected, sample, maximum_prior_change_m, method, local_budget, report
    )
    if prepared is not None:
        report["state"] = "QualifiedField"
    return prepared, reason, report
