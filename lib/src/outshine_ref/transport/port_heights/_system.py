"""Sparse whole-gradient constraints for shared rational road height patches (#169)."""

import time

import numpy as np
from scipy import sparse

from ._budget import PortLimit
from ._field import NODES


def local_matrix(columns, values, count):
    shape = values.shape
    rows = np.repeat(np.arange(np.prod(shape[:-1])), shape[-1])
    cols = np.broadcast_to(
        columns.reshape((-1,) + (1,) * (values.ndim - 2) + (6,)), shape
    )
    matrix = sparse.coo_matrix(
        (values.ravel(), (rows, cols.ravel())), shape=(int(np.prod(shape[:-1])), count)
    ).tocsr()
    matrix.eliminate_zeros()
    return matrix


def vertex_matrices(field, xy, faces, owners, *, budget=None):
    road = owners >= 0
    keys = np.unique(np.c_[faces[road].ravel(), np.repeat(owners[road], 3)], axis=0)
    ids, parts = keys.T
    first = np.r_[0, np.flatnonzero(np.diff(ids)) + 1]
    road_vertices = ids[first]
    soil_vertices = np.setdiff1d(np.arange(len(xy)), road_vertices)
    count = 3 * len(field.origins) + len(soil_vertices)
    if budget and (len(keys) > budget.max_queries or count > budget.max_variables):
        raise PortLimit("port_variable_or_sample_budget")
    columns, values, gradients, _ = field.local_basis(xy[ids], parts)
    heights = local_matrix(columns, values, count)
    derivatives = local_matrix(columns, gradients, count)
    selected = heights[first].tocoo()
    chosen = sparse.coo_matrix(
        (selected.data, (road_vertices[selected.row], selected.col)),
        shape=(len(xy), count),
    ).tocsr()
    if len(soil_vertices):
        chosen += sparse.coo_matrix(
            (
                np.ones(len(soil_vertices)),
                (soil_vertices, 3 * len(field.origins) + np.arange(len(soil_vertices))),
            ),
            shape=chosen.shape,
        ).tocsr()
    chosen_g = derivatives[(first[:, None] * 2 + [0, 1]).ravel()]
    height_delta = heights - chosen[ids]
    lookup = np.full(len(xy), -1, int)
    lookup[road_vertices] = np.arange(len(road_vertices))
    gradient_delta = derivatives - chosen_g[(lookup[ids, None] * 2 + [0, 1]).ravel()]
    equality = sparse.vstack([height_delta, gradient_delta]).tocsr()
    equality.data[abs(equality.data) < 1e-12] = 0
    equality.eliminate_zeros()
    equality = equality[np.diff(equality.indptr) != 0]
    vertex_parts = np.full(len(xy), -1, int)
    vertex_parts[road_vertices] = parts[first]
    return chosen, equality, road_vertices, vertex_parts


def continuous_matrix(field, xy, faces, owners, frames, *, max_cones=None):
    count = 3 * len(field.origins)
    blended = np.diff(field.plane_ids[owners], axis=1).ravel() != 0
    plain = np.flatnonzero(~blended)
    keys = np.c_[field.plane_ids[owners[plain], 0], frames[plain].reshape(-1, 4)]
    unique = np.unique(keys, axis=0)
    if (
        max_cones is not None
        and len(NODES) * int(blended.sum()) + len(unique) > max_cones
    ):
        raise PortLimit("port_continuous_constraint_budget")
    blocks = []
    triangles = xy[faces]
    for begin in range(0, len(faces), 256):
        selected = np.flatnonzero(blended[begin : begin + 256]) + begin
        if not len(selected):
            continue
        columns, controls, _ = field.triangle_controls(
            triangles[selected], owners[selected]
        )
        values = np.einsum("nji,nkil->nkjl", frames[selected], controls)
        blocks.append(local_matrix(columns, values, count))
    columns = np.tile(3 * unique[:, 0, None].astype(int) + np.arange(3), (1, 2))
    values = np.zeros((len(unique), 2, 6))
    values[:, :, 1:3] = unique[:, 1:].reshape(-1, 2, 2)
    if len(unique):
        blocks.append(local_matrix(columns, values, count))
    gradient = sparse.vstack(blocks).tocsc()
    if gradient.shape[0] % 2:
        raise ValueError("paired normalized gradient coordinates required")
    return gradient, {
        "blended_faces": int(blended.sum()),
        "controls_per_blended_face": len(NODES),
        "plain_unique_frames": len(unique),
        "continuous_cones": gradient.shape[0] // 2,
    }


def discrete_matrix(xy, faces, frames, heights):
    from outshine_ref.transport.heights import _gradient_weights

    coefficients = np.einsum("nki,nij->nkj", frames, _gradient_weights(xy, faces))
    rows = np.repeat(np.arange(len(faces) * 2), 3)
    columns = np.broadcast_to(faces[:, None], coefficients.shape).ravel()
    operator = sparse.coo_matrix(
        (coefficients.ravel(), (rows, columns)), shape=(2 * len(faces), len(xy))
    ).tocsr()
    return (operator @ heights).tocsc()


def assemble(field, xy, faces, owners, frames):
    began = time.perf_counter()
    height, equality, _, vertex_parts = vertex_matrices(field, xy, faces, owners)
    continuous, report = continuous_matrix(field, xy, faces, owners, frames)
    discrete = discrete_matrix(xy, faces, frames, height)
    gradient = sparse.vstack([continuous, discrete]).tocsc()
    report.update(
        unknown_plane_coefficients=height.shape[1],
        height_rows=height.shape[0],
        point_contact_equalities=equality.shape[0],
        discrete_face_cones=len(faces),
        total_cones=gradient.shape[0] // 2,
        height_matrix_nonzero=height.nnz,
        gradient_matrix_nonzero=gradient.nnz,
        sparse_bytes=sum(
            a.data.nbytes + a.indices.nbytes + a.indptr.nbytes
            for a in (height, equality, gradient)
        ),
        assembly_ms=(time.perf_counter() - began) * 1000,
    )
    return height, gradient, equality, vertex_parts, report


def polygon_fit(
    objective,
    error,
    bounds,
    fixed,
    gradient,
    *,
    seconds,
    margin,
    began,
    max_iterations=100,
):
    count = len(objective) - 1
    from scipy.optimize import linprog

    number, sides = gradient.shape[0] // 2, 16
    angles = (np.arange(sides) + 0.5) * 2 * np.pi / sides
    directions = np.c_[np.cos(angles), np.sin(angles)]
    coords = gradient.tocoo()
    rows = (coords.row[:, None] // 2 * sides + np.arange(sides)).ravel()
    columns = np.broadcast_to(coords.col[:, None], (len(coords.data), sides)).ravel()
    values = (coords.data[:, None] * directions[:, coords.row % 2].T).ravel()
    norm = sparse.coo_matrix(
        (values, (rows, columns)), shape=(number * sides, count + 1)
    ).tocsc()
    setup_ms = (time.perf_counter() - began) * 1000
    result = linprog(
        objective,
        A_ub=sparse.vstack([error, norm]).tocsc(),
        b_ub=np.r_[bounds, np.full(number * sides, margin * np.cos(np.pi / sides))],
        A_eq=fixed if fixed.shape[0] else None,
        b_eq=np.zeros(fixed.shape[0]) if fixed.shape[0] else None,
        bounds=[(None, None)] * count + [(0, None)],
        method="highs",
        options={"time_limit": seconds, "maxiter": max_iterations},
    )
    coefficients = result.x[:-1] if result.x is not None else None
    report = {
        "method": "lp",
        "status": result.message,
        "setup_ms": setup_ms,
        "iterations": result.nit,
        "polygon_sides": sides,
        "solver_ms": (time.perf_counter() - began) * 1000 - setup_ms,
    }
    return coefficients, report


def fit_planes(
    height,
    gradient,
    equality,
    targets,
    *,
    method="cone",
    seconds=8,
    margin=0.99,
    max_iterations=100,
):
    began = time.perf_counter()
    if not np.isfinite(seconds) or seconds < 0 or not 0 < margin <= 1:
        raise ValueError("finite nonnegative solver time and margin in (0,1] required")
    if seconds == 0:
        return None, {
            "valid": False,
            "reason": "time_budget_exhausted",
            "method": method,
            "total_ms": 0,
        }
    count = height.shape[1]
    datum = float(np.median(targets))
    shifted = targets - datum
    objective = np.r_[np.zeros(count), 1.0]
    rho = sparse.csc_matrix(-np.ones((len(targets), 1)))
    error = sparse.vstack(
        [sparse.hstack([height, rho]), sparse.hstack([-height, rho])]
    ).tocsc()
    bounds = np.r_[shifted, -shifted]
    fixed = sparse.hstack([equality, sparse.csc_matrix((equality.shape[0], 1))]).tocsc()
    if method == "cone":
        import clarabel

        cones = [clarabel.NonnegativeConeT(len(bounds))]
        number = gradient.shape[0] // 2
        coordinates = gradient.tocoo()
        rows = coordinates.row // 2 * 3 + coordinates.row % 2 + 1
        norm = sparse.coo_matrix(
            (-coordinates.data, (rows, coordinates.col)), shape=(number * 3, count + 1)
        ).tocsc()
        right = np.zeros((number, 3))
        right[:, 0] = margin
        cones.extend(clarabel.SecondOrderConeT(3) for _ in range(number))
        blocks, rhs = [error, norm], [bounds, right.ravel()]
        if equality.shape[0]:
            blocks.append(fixed)
            rhs.append(np.zeros(equality.shape[0]))
            cones.append(clarabel.ZeroConeT(equality.shape[0]))
        settings = clarabel.DefaultSettings()
        settings.verbose = False
        settings.max_iter, settings.max_threads, settings.time_limit = (
            max_iterations,
            1,
            seconds,
        )
        settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-9
        setup_ms = (time.perf_counter() - began) * 1000
        solver = clarabel.DefaultSolver(
            sparse.csc_matrix((count + 1, count + 1)),
            objective,
            sparse.vstack(blocks).tocsc(),
            np.concatenate(rhs),
            cones,
            settings,
        )
        result = solver.solve()
        coefficients = (
            np.asarray(result.x[:-1])
            if str(result.status)
            in ("Solved", "AlmostSolved", "MaxTime", "MaxIterations")
            else None
        )
        report = {
            "method": method,
            "status": str(result.status),
            "iterations": result.iterations,
            "solver_ms": result.solve_time * 1000,
            "setup_ms": setup_ms,
            "primal_objective": result.obj_val,
            "dual_objective": result.obj_val_dual,
        }
    elif method == "lp":
        coefficients, report = polygon_fit(
            objective,
            error,
            bounds,
            fixed,
            gradient,
            seconds=seconds,
            margin=margin,
            began=began,
            max_iterations=max_iterations,
        )
    else:
        raise ValueError("known bounded plane solver required")
    report.update(
        total_ms=(time.perf_counter() - began) * 1000,
        datum_m=datum,
        valid=False,
        reason="no_valid_plane_result",
        margin=margin,
    )
    return validated_candidate(
        coefficients, report, height, gradient, equality, shifted, margin
    )


def validated_candidate(
    coefficients, report, height, gradient, equality, shifted, margin
):
    if coefficients is not None and np.isfinite(coefficients).all():
        maximum = float(
            np.linalg.norm((gradient @ coefficients).reshape(-1, 2), axis=1).max()
        )
        contact = float(abs(equality @ coefficients).max(initial=0))
        deviation = float(abs(height @ coefficients - shifted).max())
        report.update(
            maximum_normalized_gradient_certificate=maximum,
            point_contact_error=contact,
            maximum_target_deviation_m=deviation,
        )
        report["requested_margin_excess"] = max(0.0, maximum - margin)
        if maximum <= 1 + 1e-7 and contact <= 1e-7:
            report.update(valid=True, reason="")
            return coefficients, report
    return None, report


def fit_generated(
    height, gradient, equality, targets, system, *, seconds=8, margin=0.99
):
    began = time.perf_counter()
    first = system["continuous_cones"] - system["plain_unique_frames"]
    total = gradient.shape[0] // 2
    active = np.arange(total) >= first
    reports = []
    for attempt in range(8):
        remaining = seconds - (time.perf_counter() - began)
        if remaining <= 0:
            break
        ids = np.flatnonzero(active)
        rows = (ids[:, None] * 2 + [0, 1]).ravel()
        coefficients, report = fit_planes(
            height, gradient[rows], equality, targets, seconds=remaining, margin=margin
        )
        report["active_cones"] = len(ids)
        reports.append(report)
        if coefficients is None:
            break
        norms = np.linalg.norm((gradient @ coefficients).reshape(-1, 2), axis=1)
        report["full_certificate_maximum"] = float(norms.max())
        if norms.max() <= 1 + 1e-7:
            return coefficients, dict(
                report,
                method="generated-cone",
                passes=reports.copy(),
                total_ms=(time.perf_counter() - began) * 1000,
                maximum_normalized_gradient_certificate=float(norms.max()),
            )
        violated = norms > margin + 1e-7
        new = int(np.count_nonzero(violated & ~active))
        report["new_cones"] = new
        if not new:
            break
        active |= violated
    return None, {
        "method": "generated-cone",
        "valid": False,
        "reason": "budget_without_full_certificate",
        "total_ms": (time.perf_counter() - began) * 1000,
        "passes": reports,
    }
