"""Joint bounded minimax priors; reuse existing whole-facet and landing constraints."""

import math
import time

import numpy as np
from mvt_t_profile_contacts import add_contacts, contact_residuals, prepare_contacts
from mvt_t_profile_layout import prior_values
from mvt_t_stair_loft import add_loft_equalities, height_projection, require_owned_lofts
from scipy import sparse
from scipy.optimize import linprog
from stair_junction_solve import (
    Conditions,
    facets,
    junctions,
    overlap_conditions,
    stair_condition,
)


def _prior_conditions(plan, system, datum, delta):
    targets = []
    for row, faces in zip(plan["rows"], plan["parts"], strict=True):
        vertices = np.unique(faces)
        prior = prior_values(row, plan["xy"][vertices]) - datum
        for vertex, height in zip(vertices, prior, strict=True):
            system.add([int(vertex), delta], [1, -1], height)
            system.add([int(vertex), delta], [-1, -1], -height)
        targets.append((vertices, prior + datum))
    return targets


def _check_options(
    solver,
    max_prior_change_m,
    seconds,
    contact_presolve,
    height_formulation,
    gradient_model,
    selection,
):
    if solver not in ("highs-ds", "highs-ipm"):
        raise ValueError("known HiGHS method required")
    if not math.isfinite(max_prior_change_m) or not 0 <= max_prior_change_m <= 5:
        raise ValueError("bounded nonnegative prior-change recipe required")
    if not math.isfinite(seconds) or not 0 <= seconds <= 60:
        raise ValueError("bounded nonnegative cooperative solver time required")
    if type(contact_presolve) is not bool:
        raise ValueError("explicit contact presolve choice required")
    if height_formulation not in ("projected", "equalities"):
        raise ValueError("known height formulation required")
    if gradient_model not in ("whole_norm", "axis_crossfall"):
        raise ValueError("known declared profile gradient model required")
    if selection not in ("minimax", "minimax_then_variation"):
        raise ValueError("known declared profile solution selection required")


def _gradient_conditions(
    plan, system, normals, reserve, height_bound, gradient_model, crossfall_limit
):
    directional_work = {}
    if gradient_model == "axis_crossfall":
        from mvt_t_directional_facets import directional_facets

        directional_work = directional_facets(
            plan,
            system,
            normals,
            reserve,
            True,
            True,
            height_bound_m=height_bound,
            crossfall_limit=crossfall_limit,
        )
    else:
        facets(plan, system, normals, reserve, True, True, height_bound_m=height_bound)
    return {
        "gradient_model": gradient_model,
        **(
            {"crossfall_limit": crossfall_limit, "directional_work": directional_work}
            if directional_work
            else {}
        ),
    }


def _sparse_bytes(matrix):
    return sum(a.nbytes for a in (matrix.data, matrix.indices, matrix.indptr))


def _matrix_note(matrix, projection):
    return {
        "variables": matrix.shape[1],
        "unreduced_variables": projection.shape[0],
        "projection_array_bytes": _sparse_bytes(projection),
        "constraints": matrix.shape[0],
        "nonzeros": matrix.nnz,
        "sparse_array_bytes": _sparse_bytes(matrix),
    }


def _project_conditions(plan, system, formulation):
    full = system.matrix()
    projection = (
        height_projection(plan, full.shape[1])
        if formulation == "projected"
        else sparse.eye(full.shape[1], format="csc")
    )
    return full, projection, (full @ projection).tocsc()


def _linear_fit(
    matrix,
    projection,
    error_variable,
    rhs,
    max_prior_change_m,
    seconds,
    solver,
    presolve,
):
    objective = np.zeros(projection.shape[0])
    objective[error_variable] = 1
    objective = np.asarray(projection.T @ objective).ravel()
    bounds = [(None, None)] * (matrix.shape[1] - 1) + [(0, max_prior_change_m)]
    prepared = time.perf_counter()
    answer = linprog(
        objective,
        A_ub=matrix,
        b_ub=rhs,
        bounds=bounds,
        method=solver,
        options={
            "time_limit": seconds,
            "primal_feasibility_tolerance": 1e-9,
            "presolve": presolve,
        },
    )
    return answer, prepared, bounds


def _select_solution(
    plan, matrix, rhs, projection, answer, bounds, selection, solver, deadline
):
    if selection == "minimax":
        return answer.x, {}
    if not answer.success:
        return answer.x, {"selected": False, "status": "primary_not_valid"}
    from stair_profile_variation import select_variation

    return select_variation(
        plan, matrix, rhs, projection, answer.x, answer.fun, bounds, solver, deadline
    )


def solve_profiles(
    plan,
    *,
    max_prior_change_m=2.0,
    seconds=5.0,
    solver="highs-ds",
    fixed_contacts=(),
    max_contact_samples=32768,
    contact_method="index",
    contact_row_scale=1024.0,
    contact_presolve=False,
    height_formulation="projected",
    gradient_model="whole_norm",
    crossfall_limit=0.2,
    selection="minimax",
):
    _check_options(
        solver,
        max_prior_change_m,
        seconds,
        contact_presolve,
        height_formulation,
        gradient_model,
        selection,
    )
    require_owned_lofts(plan)
    began = time.perf_counter()
    count, planes = len(plan["xy"]), 3 * len(plan["nodes"])
    if count > 4096 or sum(map(len, plan["parts"])) > 8192:
        raise ValueError("local whole-profile geometry budget exceeded")
    delta = count + planes
    system = Conditions(delta + 1)
    datum = float(
        np.median(np.concatenate([r["height_prior_xyz_m"][:, 2] for r in plan["rows"]]))
    )
    contacts, contact_work = prepare_contacts(
        plan, fixed_contacts, max_samples=max_contact_samples, method=contact_method
    )
    add_contacts(system, contacts, datum, row_scale=contact_row_scale)
    targets = _prior_conditions(plan, system, datum, delta)
    angles = np.arange(16) * 2 * np.pi / 16
    normals, reserve = np.c_[np.cos(angles), np.sin(angles)], float(np.cos(np.pi / 16))
    height_bound = max(abs(p).max() for _v, p in targets) + max_prior_change_m
    gradient_note = _gradient_conditions(
        plan, system, normals, reserve, height_bound, gradient_model, crossfall_limit
    )
    junctions(plan, system, normals, reserve, True, plane_offset=count)
    constant_loft_rows = 0
    for stair in plan["stairs"]:
        constant_loft_rows += stair_condition(
            plan,
            system,
            *stair,
            plane_offset=count,
            direction=plan["stair_directions"][stair[0]],
            height_lofts=plan.get("stair_lofts", ()),
        )
    overlaps = overlap_conditions(plan, system, 0)
    if height_formulation == "equalities":
        add_loft_equalities(plan, system)
    full_matrix, projection, matrix = _project_conditions(
        plan, system, height_formulation
    )
    answer, prepared, bounds = _linear_fit(
        matrix,
        projection,
        delta,
        system.bounds,
        max_prior_change_m,
        seconds,
        solver,
        contact_presolve if contacts else True,
    )
    primary_finished = time.perf_counter()
    selected, selection_report = _select_solution(
        plan,
        matrix,
        system.bounds,
        projection,
        answer,
        bounds,
        selection,
        solver,
        began + seconds,
    )
    note = {
        "valid": bool(answer.success),
        "status": answer.message,
        **_matrix_note(matrix, projection),
        "height_formulation": height_formulation,
        **gradient_note,
        "preparation_s": prepared - began,
        "solver_s": primary_finished - prepared,
        "elapsed_s": time.perf_counter() - began,
        "max_prior_change_budget_m": max_prior_change_m,
        "contact_work": contact_work,
        "contact_row_scale": contact_row_scale,
        "presolve": contact_presolve if contacts else True,
        "overlap_vertex_constraints": overlaps,
        "constant_loft_monotonic_rows_skipped": constant_loft_rows,
        "scope": "local finite profile proposal; only explicitly supplied fixed contacts are bound",
        **(
            {"solution_selection": selection, "selection_report": selection_report}
            if selection_report
            else {}
        ),
    }
    if not answer.success:
        return None, note
    solution = projection @ selected
    heights = solution[:count] + datum
    note.update(
        max_prior_change_m=float(max(abs(heights[v] - p).max() for v, p in targets)),
        objective=float(answer.fun),
        maximum_constraint_residual=float(
            np.max(full_matrix @ solution - system.bounds)
        ),
        junction_planes=(solution[count:delta].reshape(-1, 3) + [datum, 0, 0]).tolist(),
        fixed_contact_residuals_m=contact_residuals(contacts, heights),
    )
    return heights, note
