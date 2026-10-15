"""Outer LP cuts for complete paired gradient cones; never accept an unchecked LP."""

import time
from dataclasses import asdict

import numpy as np
from outshine_ref.transport.port_heights._system import validated_candidate
from port_highs_work import CutLimit, SparseLP, Work


def support_groups(gradient, work):
    count = gradient.shape[0] // 2
    groups = work.array(np.empty(count, np.int64))
    keys = {}
    for index in range(count):
        key = gradient.indices[
            gradient.indptr[2 * index] : gradient.indptr[2 * index + 2]
        ].tobytes()
        groups[index] = keys.setdefault(key, len(keys))
    work.support_key_calls += count
    work.support_key_bytes += gradient.indices.nbytes
    work.explicit_value_copy_bytes += gradient.indices.nbytes
    return groups, len(keys)


def selected_cones(norms, groups, selection, margin, work):
    ids = work.array(np.flatnonzero(norms > margin + 1e-7))
    if selection == "all" or not len(ids):
        return ids
    labels, count = groups
    maximum = work.array(np.full(count, -np.inf))
    np.maximum.at(maximum, labels[ids], norms[ids])
    candidates = work.array(ids[norms[ids] == maximum[labels[ids]]])
    _, first = np.unique(labels[candidates], return_index=True)
    return work.array(candidates[first])


def cut_rows(gradient, ids, coordinates, norms, work):
    direction = work.array(coordinates[ids] / norms[ids, None])
    first = gradient[2 * ids]
    second = gradient[2 * ids + 1]
    if np.shares_memory(first.data, gradient.data):
        first = first.copy()
    if np.shares_memory(second.data, gradient.data):
        second = second.copy()
    work.matrix(first, copied_values=True)
    work.matrix(second, copied_values=True)
    first.data *= work.array(
        np.repeat(direction[:, 0], np.diff(first.indptr)), copied=True
    )
    second.data *= work.array(
        np.repeat(direction[:, 1], np.diff(second.indptr)), copied=True
    )
    result = work.matrix(first + second)
    result.eliminate_zeros()
    return result


def validate_problem(
    height,
    gradient,
    equality,
    targets,
    seconds,
    margin,
    max_iterations,
    max_nonzeros,
    solver,
    selection,
    method,
):
    if (
        method != "cone"
        or solver not in ("ipm", "simplex")
        or selection not in ("all", "support")
        or not np.isfinite((seconds, margin)).all()
        or seconds < 0
        or not 0 < margin <= 1
        or type(max_iterations) is not int
        or max_iterations <= 0
        or type(max_nonzeros) is not int
        or max_nonzeros <= 0
    ):
        raise ValueError(
            "bounded conic formulation and known delayed LP policy required"
        )
    if (
        gradient.shape[0] % 2
        or gradient.shape[1] != height.shape[1]
        or equality.shape[1] != height.shape[1]
    ):
        raise ValueError(
            "paired gradient/equality columns must match height coefficients"
        )
    if height.shape[0] == 0 or height.shape[1] == 0 or gradient.shape[0] == 0:
        raise ValueError("nonempty observations, variables and gradient cones required")
    if (
        np.asarray(targets).shape != (height.shape[0],)
        or not np.isfinite(targets).all()
    ):
        raise ValueError("one finite target per height observation required")


def inspect_iterate(values, status, height, gradient, equality, shifted, margin, work):
    if status.rsplit(".", 1)[-1] not in ("kOptimal", "kTimeLimit", "kIterationLimit"):
        raise CutLimit("delayed_lp_subproblem_failed")
    if values is None or not np.isfinite(values).all():
        raise CutLimit("delayed_lp_no_finite_iterate")
    coefficients = values[:-1]
    deviation = float(abs(height @ coefficients - shifted).max())
    work.height_nonzeros_visited += height.nnz
    if values[-1] < -1e-7 or deviation > values[-1] + 1e-7:
        raise CutLimit("delayed_lp_infeasible_error_epigraph")
    coordinates = work.array((gradient @ coefficients).reshape(-1, 2))
    norms = work.array(np.hypot(coordinates[:, 0], coordinates[:, 1]))
    checked, note = validated_candidate(
        coefficients, {}, height, gradient, equality, shifted, margin
    )
    note["lp_epigraph_rho_m"] = float(values[-1])
    work.gradient_nonzeros_visited += 2 * gradient.nnz
    work.full_cones_checked += 2 * len(norms)
    work.height_nonzeros_visited += height.nnz
    work.equality_nonzeros_visited += equality.nnz
    return checked, note, coordinates, norms


def fit_delayed(
    height,
    gradient,
    equality,
    targets,
    *,
    seconds=8.0,
    margin=0.99,
    max_iterations=100,
    max_nonzeros=12000000,
    solver="ipm",
    selection="support",
    method="cone",
):
    began = time.perf_counter()
    validate_problem(
        height,
        gradient,
        equality,
        targets,
        seconds,
        margin,
        max_iterations,
        max_nonzeros,
        solver,
        selection,
        method,
    )
    datum = float(np.median(targets))
    shifted = np.asarray(targets) - datum
    report = {
        "method": "highs-delayed-" + solver,
        "cut_selection": selection,
        "valid": False,
        "reason": "delayed_lp_time_budget",
        "datum_m": datum,
        "margin": margin,
        "passes": [],
        "iterations": 0,
        "cuts": 0,
        "solver_ms": 0.0,
    }
    work, model, candidate = Work(), None, None
    try:
        if seconds == 0:
            raise CutLimit("delayed_lp_time_budget")
        retained = height.nnz + gradient.nnz + equality.nnz + gradient.nnz
        work.check(retained, max_nonzeros)
        rows = work.matrix(gradient.tocsr(copy=True), copied_values=True)
        sizes = work.array(np.diff(rows.indptr))
        groups = support_groups(rows, work) if selection == "support" else None
        model = SparseLP(
            height, equality, shifted, solver, work, max_nonzeros, retained
        )
        model.seed_box(rows, margin)
        report["initial_outer_relaxation"] = (
            "All paired gradient coordinates bounded by +/-margin; imported in 4096-row blocks."
        )
        report["setup_ms"] = (time.perf_counter() - began) * 1000
        for step in range(max_iterations):
            remaining = seconds - report["solver_ms"] / 1000
            left = max_iterations - report["iterations"]
            if remaining <= 0 or left <= 0:
                raise CutLimit("delayed_lp_time_or_iteration_budget")
            solve_began = time.perf_counter()
            values, used, status = model.solve(remaining, left)
            report["solver_ms"] += (time.perf_counter() - solve_began) * 1000
            report["iterations"] += used
            report["status"] = status
            checked, note, coordinates, norms = inspect_iterate(
                values, status, height, rows, equality, shifted, margin, work
            )
            if checked is not None:
                candidate = work.array(checked.copy(), copied=True)
                report["candidate_round"] = step
                report.update(
                    {
                        k: v
                        for k, v in note.items()
                        if k not in ("passes", "iterations", "cuts")
                    }
                )
            ids = selected_cones(norms, groups, selection, margin, work)
            report["passes"].append(
                {
                    "round": step,
                    "status": status,
                    "lp_iterations": used,
                    "maximum_full_norm": float(norms.max(initial=0)),
                    "selected_cuts": len(ids),
                }
            )
            if not len(ids):
                break
            pending = int(sizes[2 * ids].sum() + sizes[2 * ids + 1].sum())
            work.check(retained + model.nonzeros + 2 * pending, max_nonzeros)
            cuts = cut_rows(rows, ids, coordinates, norms, work)
            model.add(
                cuts,
                work.array(np.full(len(ids), -np.inf)),
                work.array(np.full(len(ids), margin)),
            )
            report["cuts"] += len(ids)
        else:
            raise CutLimit("delayed_lp_round_budget")
    except CutLimit as error:
        report["termination"] = str(error)
    return finish_fit(candidate, model, report, work, began)


def finish_fit(candidate, model, report, work, began):
    if model is not None:
        report["solver_version"] = model.highs.version()
        report["lp_nonzeros"] = model.nonzeros
        model.highs.clear()
    report.update(
        total_ms=(time.perf_counter() - began) * 1000,
        work=asdict(work),
        internal_native_allocation_count=None,
        internal_native_copy_bytes=None,
    )
    if candidate is None:
        report.update(
            valid=False, reason=report.get("termination", "delayed_lp_no_valid_result")
        )
    else:
        report.update(valid=True, reason="")
    return candidate, report
