"""Independent linear-program reference for a chosen piecewise-constant flow field."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import shapely
from scipy import sparse
from scipy.optimize import linprog
from shapely.strtree import STRtree

from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import HeightFit

from river_profiles import RiverProfile
from river_linear_model import linear_system
from river_surface import source_body, surface_mesh


def infer_flow(mesh, segments, levels):
    segments, levels = np.asarray(segments, dtype=float), np.asarray(levels, dtype=float)
    delta = levels[:, 1] - levels[:, 0]
    selected = np.flatnonzero(np.abs(delta) > .01)
    if not len(selected):
        raise ValueError("no measured fall for a flow-direction hypothesis")
    lines = shapely.linestrings(segments[selected])
    centre = shapely.points(mesh.vertices[mesh.triangles, :2].mean(axis=1))
    nearest = STRtree(lines).nearest(centre)
    source = selected[nearest]
    directions = (segments[source, 1] - segments[source, 0]) * -np.sign(delta[source, None])
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    return directions, source, shapely.distance(centre, lines[nearest])


def face_gradients(mesh, heights):
    points = mesh.vertices[mesh.triangles, :2]
    matrix = np.stack((points[:, 1] - points[:, 0], points[:, 2] - points[:, 0]), axis=1)
    differences = heights[mesh.triangles][:, 1:] - heights[mesh.triangles][:, :1]
    return np.linalg.solve(matrix, differences[..., None])[..., 0]


def source_face_rows(mesh, segments, levels, down_grade=.1, row_limit=32768):
    segments, levels = np.asarray(segments, dtype=float), np.asarray(levels, dtype=float)
    selected = np.flatnonzero(np.abs(levels[:, 1] - levels[:, 0]) > .01)
    faces = shapely.polygons(mesh.vertices[mesh.triangles, :2])
    tree = STRtree(faces)
    rows, sources = [], []
    outside, lengths = [], []
    domain = shapely.union_all(faces)
    for first in range(0, len(selected), 64):
        ids = selected[first:first + 64]
        lines = shapely.linestrings(segments[ids])
        lengths.extend(shapely.length(lines))
        outside.extend(shapely.length(shapely.difference(lines, domain)))
        line, face = tree.query(lines, predicate="intersects")
        overlap = shapely.length(shapely.intersection(lines[line], faces[face]))
        line, face = line[overlap > 0], face[overlap > 0]
        if sum(len(part) for part in rows) + len(face) > row_limit:
            raise ValueError("source flow constraint budget exhausted")
        source = ids[line]
        direction = (segments[source, 1] - segments[source, 0])
        direction *= -np.sign(levels[source, 1] - levels[source, 0])[:, None]
        direction /= np.linalg.norm(direction, axis=1)[:, None]
        rows.append(np.c_[face, direction, np.full(len(face), -down_grade), np.zeros(len(face))])
        sources.append(source)
    rows = np.concatenate(rows) if rows else np.empty((0, 5))
    sources = np.concatenate(sources) if sources else np.empty(0, dtype=int)
    return rows, sources, {"falling_source_segments": len(selected),
                           "direction_unknown_segments": len(segments) - len(selected),
                           "directed_rows": len(rows),
                           "source_length_m": float(np.sum(lengths)),
                           "inside_domain_length_m": float(np.sum(lengths) - np.sum(outside)),
                           "outside_domain_length_m": float(np.sum(outside)),
                           "partly_outside_source_segments": selected[np.asarray(outside) > 1e-6].tolist()}


def optimality_certificate(solution, objective, constraints, bounds, contacts, contact_heights):
    if not solution.success:
        return {"solver_optimal": False}
    inequality = solution.ineqlin.marginals
    lower, upper = solution.lower.marginals, solution.upper.marginals
    dual = float(bounds @ inequality)
    stationarity = constraints.T @ inequality + lower + upper - objective
    if contacts is not None:
        equality = solution.eqlin.marginals
        dual += float(contact_heights @ equality)
        stationarity += contacts.T @ equality
    gap = abs(float(solution.fun) - dual)
    residual = float(np.max(np.abs(stationarity)))
    sign_violation = max(float(inequality.max()), float((-lower).max()), float(upper.max()), 0.0)
    unbounded_marginal = max(float(np.abs(lower[:-1]).max(initial=0)),
                            float(np.abs(upper).max(initial=0)))
    error_violation = max(float((constraints @ solution.x - bounds)[:2 * (len(objective) - 1)].max()),
                          -float(solution.x[-1]), 0.0)
    return {"dual_objective_error_m": dual, "dual_gap_m": gap,
            "dual_stationarity_residual": residual, "dual_sign_violation": sign_violation,
            "free_variable_marginal": unbounded_marginal,
            "prior_error_row_violation_m": error_violation,
            "solver_optimal": max(gap, residual, sign_violation, unbounded_marginal,
                                   error_violation) <= 1e-7}


def fit_surface(mesh, flow, *, down_grade=.1, cross_grade=.005, fixed=None, directed_rows=None,
                seconds=2.0, iterations=20000, vertex_limit=8192, face_limit=16384):
    began = time.perf_counter()
    report = {"valid": False, "solver_optimal": False,
              "scope": "independent LP reference, not a native recipe or source-semantic admission"}

    def absent(reason):
        return HeightFit(None, dict(report, reason=reason, total_ms=(time.perf_counter() - began) * 1000))

    if not isinstance(mesh, Mesh):
        raise TypeError("owned shared Mesh required")
    if not all(np.isfinite(x) and x >= 0 for x in [down_grade, cross_grade, seconds]):
        raise ValueError("finite nonnegative grades and time budget required")
    nv, nf = len(mesh.vertices), len(mesh.triangles)
    if not nf:
        return absent("empty_surface")
    if nv > vertex_limit or nf > face_limit:
        return absent("input_budget_exhausted")
    if seconds == 0 or iterations <= 0:
        return absent("solver_budget_exhausted")
    flow = np.asarray(flow, dtype=float)
    if flow.shape != (nf, 2) or not np.isfinite(flow).all():
        raise ValueError("one finite local XY flow direction per face required")
    lengths = np.linalg.norm(flow, axis=1)
    if np.any(lengths == 0) or not np.isfinite(lengths).all():
        raise ValueError("nonzero representable flow directions required")
    flow = flow / lengths[:, None]
    cross = np.c_[-flow[:, 1], flow[:, 0]]
    rows = np.empty((0, 5)) if directed_rows is None else np.asarray(directed_rows, dtype=float)
    if (rows.ndim != 2 or rows.shape[1] != 5 or not np.isfinite(rows).all() or
            np.any(rows[:, 0] != np.floor(rows[:, 0])) or np.any(rows[:, 0] < 0) or
            np.any(rows[:, 0] >= nf) or np.any(rows[:, 3] > rows[:, 4])):
        raise ValueError("directed rows require existing face, XY direction and gradient interval")
    if len(rows) > 32768:
        return absent("directed_row_budget_exhausted")
    row_faces = rows[:, 0].astype(int)
    row_length = np.linalg.norm(rows[:, 1:3], axis=1)
    if np.any(row_length == 0) or not np.isfinite(row_length).all():
        raise ValueError("nonzero representable directed-row vectors required")
    row_direction = rows[:, 1:3] / row_length[:, None]
    pins = dict(fixed or {})
    if any(type(i) is not int or not 0 <= i < nv or not np.isfinite(h) for i, h in pins.items()):
        raise ValueError("finite fixed heights at existing vertices required")
    try:
        constraints, bounds, datum, target = linear_system(
            mesh, flow, rows, down_grade, cross_grade)
    except ValueError:
        return absent("degenerate_surface")
    indices = list(pins)
    contacts = (sparse.coo_matrix((np.ones(len(indices)), (np.arange(len(indices)), indices)),
                                  shape=(len(indices), nv + 1)).tocsr() if pins else None)
    contact_heights = np.asarray(list(pins.values())) - datum if pins else None
    objective = np.r_[np.zeros(nv), 1.0]
    setup_ms = (time.perf_counter() - began) * 1000
    solution = linprog(objective, A_ub=constraints, b_ub=bounds,
                       A_eq=contacts, b_eq=contact_heights,
                       bounds=[(None, None)] * nv + [(0, None)], method="highs-ds",
                       options={"time_limit": seconds, "maxiter": iterations,
                                "primal_feasibility_tolerance": 1e-8,
                                "dual_feasibility_tolerance": 1e-8})
    report.update(vertices=nv, faces=nf, rows=constraints.shape[0], nonzeros=constraints.nnz,
                  constraint_matrix_bytes=(constraints.data.nbytes + constraints.indices.nbytes +
                                           constraints.indptr.nbytes),
                  solver_status=solution.message, iterations=solution.nit, setup_ms=setup_ms,
                  solver_ms=(time.perf_counter() - began) * 1000 - setup_ms)
    if solution.x is None:
        return absent("no_feasible_solution")
    height = solution.x[:-1] + datum
    gradients = face_gradients(mesh, height)
    long_grade, side_grade = (gradients * flow).sum(axis=1), (gradients * cross).sum(axis=1)
    pin_error = max((abs(height[i] - h) for i, h in pins.items()), default=0.0)
    violation = max(float(long_grade.max()), float((-long_grade - down_grade).max()),
                    float((np.abs(side_grade) - cross_grade).max()), 0.0)
    directed_grade = (gradients[row_faces] * row_direction).sum(axis=1)
    directed_violation = max(float((directed_grade - rows[:, 4]).max(initial=0)),
                             float((rows[:, 3] - directed_grade).max(initial=0)))
    report.update(maximum_uphill_grade=float(long_grade.max()),
                  maximum_downhill_grade=float((-long_grade).max()),
                  maximum_cross_grade=float(np.abs(side_grade).max()), fixed_height_error_m=pin_error,
                  maximum_prior_error_m=float(np.abs(height - mesh.vertices[:, 2]).max()),
                  maximum_grade_violation_m_per_m=violation,
                  directed_rows=len(rows), directed_row_violation_m_per_m=directed_violation,
                  objective_error_m=float(solution.x[-1]))
    if (not np.isfinite(height).all() or not np.isfinite(gradients).all() or
            violation > 1e-7 or pin_error > 1e-7 or directed_violation > 1e-7):
        return absent("independent_whole_face_check_failed")
    report.update(optimality_certificate(solution, objective, constraints, bounds,
                                         contacts, contact_heights))
    vertices = mesh.vertices.copy()
    vertices[:, 2] = height
    report.update(valid=True, reason="", total_ms=(time.perf_counter() - began) * 1000)
    return HeightFit(Mesh(vertices, mesh.triangles), report)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--spacing-m", type=float, default=20.0)
    parser.add_argument("--down-grade", type=float, default=.1)
    parser.add_argument("--cross-grade", type=float, default=.005)
    parser.add_argument("--seconds", type=float, default=2.0)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private results belong outside the checkout")
    began = time.perf_counter()
    with np.load(args.input, allow_pickle=False) as data:
        body = source_body(data)
        flat, topology = surface_mesh(body, args.spacing_m)
        target, references = RiverProfile(data["segment_nodes"], data["segment_levels"]).sample(
            flat.vertices[:, :2], "blend-affine")
        if not np.isfinite(target).all():
            parser.error("the chosen prior is incomplete on this body")
        mesh = Mesh(np.c_[flat.vertices[:, :2], target], flat.triangles)
        flow, source, distances = infer_flow(mesh, data["segment_nodes"], data["segment_levels"])
        directed_rows, directed_source, coverage = source_face_rows(
            mesh, data["segment_nodes"], data["segment_levels"], args.down_grade)
    fit = fit_surface(mesh, flow, down_grade=args.down_grade, cross_grade=args.cross_grade,
                      directed_rows=directed_rows, seconds=args.seconds)
    report = dict(fit.report, topology=topology, maximum_prior_references=int(references.max()),
                  input_sha256=hashlib.sha256(args.input.read_bytes()).hexdigest(),
                  directions="inferred hypotheses; not supplied OSM directions",
                  maximum_direction_source_distance_m=float(distances.max()),
                  downstream_grade_limit=args.down_grade, cross_grade_limit=args.cross_grade,
                  source_path_coverage=coverage,
                  pipeline_ms=(time.perf_counter() - began) * 1000)
    output.mkdir(parents=True, exist_ok=True)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if fit.mesh is not None:
        np.savez_compressed(output / "surface.npz", vertices=fit.mesh.vertices,
                            triangles=fit.mesh.triangles, prior=mesh.vertices[:, 2],
                            flow=flow, source_segment=source, directed_rows=directed_rows,
                            directed_source_segment=directed_source)
    print(json.dumps(report))
    raise SystemExit(0 if fit.mesh is not None else 1)


if __name__ == "__main__":
    main()
