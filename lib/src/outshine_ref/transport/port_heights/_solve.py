"""Bounded coupled plane fitting and parameter-aligned finite mesh planning."""

import time
from copy import deepcopy

import numpy as np
from scipy.spatial import cKDTree

from ._budget import DEFAULT_BUDGET, PortLimit
from ._fit import fit_sliced
from ._quality import quality
from ._slices import initial_knots, slice_mesh
from ._system import continuous_matrix, vertex_matrices
from ._topology import topology_audit
from ._world import normal_gradients


def apply_seed(model, mesh, seed, *, budget=None):
    h, equal, _, parts = vertex_matrices(
        model, mesh["xy"], mesh["faces"], mesh["parts"], budget=budget
    )
    soil = np.flatnonzero(parts < 0)
    distance, nearest = cKDTree(seed["vertices"][:, :2]).query(mesh["xy"][soil])
    if np.max(distance, initial=0) != 0:
        raise ValueError("new unconstrained soil point needs a coupled refit")
    coefficients = np.r_[
        seed["plane_coefficients"], seed["vertices"][nearest, 2] - seed["datum_m"]
    ]
    mesh["heights"] = h @ coefficients + seed["datum_m"]
    error = float(abs(equal @ coefficients).max(initial=0))
    if error > 1e-7:
        raise ValueError("slice source contacts violate their shared plane equalities")
    return coefficients, error


def add_knots(knots, mesh, marked, *, max_stations=512):
    changes = 0
    for part in np.unique(mesh["parts"][marked]):
        if part not in knots:
            continue
        intervals = mesh["intervals"][marked & (mesh["parts"] == part)]
        mid = np.unique(intervals.mean(axis=1))
        updated = np.unique(np.r_[knots[part], mid[np.isfinite(mid)]])
        if len(updated) > max_stations:
            raise PortLimit("port_part_station_budget")
        changes += len(updated) - len(knots[part])
        knots[part] = updated
    return changes


def solve_slices(
    model,
    product,
    captured,
    original,
    seed,
    *,
    budget=DEFAULT_BUDGET,
    method="cone",
    fit=True,
    progress=None,
    records=None,
    began=None,
):
    began = time.monotonic() if began is None else began
    records = [] if records is None else records
    remaining = lambda: budget.time_limit_s - (time.monotonic() - began)
    if remaining() <= 0:
        raise PortLimit("port_time_budget")
    knots = initial_knots(
        model,
        original,
        budget.initial_segments,
        max_stations=budget.max_stations_per_part,
    )
    fits = 0
    certificates = (
        continuous_matrix(
            model,
            product["vertices"],
            product["indices"],
            product["face_owners"],
            captured["road_frames"],
            max_cones=budget.max_cones,
        )
        if fit
        else None
    )
    for iteration in range(budget.max_rounds):
        if remaining() <= 0:
            raise PortLimit("port_time_budget")
        mesh, note = slice_mesh(
            model,
            original,
            product,
            captured,
            knots,
            seconds=min(budget.geometry_time_limit_s, remaining()),
            max_vertices=budget.max_output_vertices,
            max_faces=budget.max_output_faces,
            max_weld_pairs=budget.max_weld_pairs,
        )
        note["topology"] = topology_audit(original, mesh)
        coefficients, equality_error = apply_seed(model, mesh, seed, budget=budget)
        soil = mesh["parts"] < 0
        gradients = normal_gradients(mesh["xy"], mesh["faces"][soil], mesh["heights"])
        soil_grade = np.linalg.norm(
            np.einsum("nki,ni->nk", mesh["frames"][soil], gradients), axis=1
        )
        needs_fit = (
            fit
            and fits < budget.max_fits
            and (iteration == 0 or np.any(soil_grade > 1.0000001))
        )
        fitting = None
        if needs_fit:
            if remaining() <= 0:
                raise PortLimit("port_time_budget")
            fitted, system, solver = fit_sliced(
                model,
                captured,
                mesh,
                certificates,
                seconds=min(budget.solver_time_limit_s, remaining()),
                method=method,
                budget=budget,
            )
            fitting = {"system": system, "solver": solver}
            fits += 1
            if progress:
                progress(
                    deepcopy(dict(stage="coupled-fit", iteration=iteration, **fitting))
                )
            if fitted is not None:
                seed, coefficients = fitted
        marked, audit, errors = quality(
            model,
            seed["plane_coefficients"],
            mesh,
            product["part_roles"],
            refinement_reserve=0.8 if fit else 1.0,
        )
        record = {
            "iteration": iteration,
            "mesh": note,
            "quality": audit,
            "equality_error": equality_error,
            "elapsed_ms": (time.monotonic() - began) * 1000,
            "fitting": fitting,
        }
        records.append(record)
        if progress:
            progress(deepcopy(record))
        if fitting and not fitting["solver"]["valid"]:
            break
        if audit["geometry_complete"] or iteration == budget.max_rounds - 1:
            break
        if not add_knots(
            knots, mesh, marked, max_stations=budget.max_stations_per_part
        ):
            break
    return mesh, coefficients, seed, marked, errors, records
