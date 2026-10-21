"""Joint bounded minimax priors; reuse existing whole-facet and landing constraints."""

import math
import time

import numpy as np
from mvt_t_profile_contacts import add_contacts, contact_residuals, prepare_contacts
from mvt_t_profile_layout import prior_values
from mvt_t_stair_loft import add_loft_equalities, height_projection
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
    facets(plan, system, normals, reserve, True, True, height_bound_m=height_bound)
    junctions(plan, system, normals, reserve, True, plane_offset=count)
    for stair in plan["stairs"]:
        stair_condition(
            plan,
            system,
            *stair,
            plane_offset=count,
            direction=plan["stair_directions"][stair[0]],
        )
    overlaps = overlap_conditions(plan, system, 0)
    if height_formulation == "equalities":
        add_loft_equalities(plan, system)
    full_matrix = system.matrix()
    projection = (
        height_projection(plan, delta + 1)
        if height_formulation == "projected"
        else sparse.eye(delta + 1, format="csc")
    )
    matrix = (full_matrix @ projection).tocsc()
    objective = np.zeros(delta + 1)
    objective[delta] = 1
    objective = np.asarray(projection.T @ objective).ravel()
    prepared = time.perf_counter()
    answer = linprog(
        objective,
        A_ub=matrix,
        b_ub=system.bounds,
        bounds=[(None, None)] * (matrix.shape[1] - 1) + [(0, max_prior_change_m)],
        method=solver,
        options={
            "time_limit": seconds,
            "primal_feasibility_tolerance": 1e-9,
            "presolve": contact_presolve if contacts else True,
        },
    )
    note = {
        "valid": bool(answer.success),
        "status": answer.message,
        "variables": matrix.shape[1],
        "unreduced_variables": delta + 1,
        "height_formulation": height_formulation,
        "projection_array_bytes": projection.data.nbytes
        + projection.indices.nbytes
        + projection.indptr.nbytes,
        "constraints": matrix.shape[0],
        "nonzeros": matrix.nnz,
        "preparation_s": prepared - began,
        "solver_s": time.perf_counter() - prepared,
        "elapsed_s": time.perf_counter() - began,
        "max_prior_change_budget_m": max_prior_change_m,
        "contact_work": contact_work,
        "contact_row_scale": contact_row_scale,
        "presolve": contact_presolve if contacts else True,
        "overlap_vertex_constraints": overlaps,
        "sparse_array_bytes": matrix.data.nbytes
        + matrix.indices.nbytes
        + matrix.indptr.nbytes,
        "scope": "local finite profile proposal; only explicitly supplied fixed contacts are bound",
    }
    if not answer.success:
        return None, note
    solution = projection @ answer.x
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
