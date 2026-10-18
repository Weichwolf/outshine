"""Same real water geometry and directed constraints, LP minimax versus weighted QP."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path
from unittest.mock import patch

import directional_river_fit as reference
import numpy as np
import weighted_river_fit as weighted
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from river_profiles import RiverProfile
from river_surface import source_body, surface_mesh
from stair_terrain_experiment import frozen


def inputs(path, spacing, mode):
    with np.load(path, allow_pickle=False) as saved:
        flat, topology = surface_mesh(source_body(saved), spacing)
        target, _references = RiverProfile(
            saved["segment_nodes"], saved["segment_levels"]
        ).sample(flat.vertices[:, :2], "blend-affine")
        mesh = Mesh(np.c_[flat.vertices[:, :2], target], flat.triangles)
        flow, _source, _distances = reference.infer_flow(
            mesh, saved["segment_nodes"], saved["segment_levels"]
        )
        rows, source, coverage = reference.source_face_rows(
            mesh, saved["segment_nodes"], saved["segment_levels"]
        )
        segments, levels = saved["segment_nodes"], saved["segment_levels"]
    triangles = mesh.vertices[mesh.triangles, :2]
    u, v = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    area = 0.5 * np.abs(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0])
    weights = (
        np.bincount(
            mesh.triangles.ravel(),
            weights=np.repeat(area / 3, 3),
            minlength=len(mesh.vertices),
        )
        if mode == "area"
        else None
    )
    weights = weighted.objective_weights(weights, len(mesh.vertices))
    return mesh, flow, rows, source, weights, topology, coverage, segments, levels


def path_check(mesh, segments, levels):
    selected = np.flatnonzero(np.abs(np.diff(levels, axis=1)[:, 0]) > 0.01)
    points = segments[selected]
    descending = levels[selected, 1] <= levels[selected, 0]
    points = np.where(descending[:, None, None], points, points[:, ::-1])
    t = np.linspace(0, 1, 65)
    xy = points[:, :1] * (1 - t[None, :, None]) + points[:, 1:] * t[None, :, None]
    contact = SurfaceSampler(mesh).sample(xy.reshape(-1, 2))
    z = contact.points_enu_m[:, 2].reshape(len(selected), 65)
    valid = np.array([r == "" for r in contact.reasons]).reshape(len(selected), 65)
    pairs = valid[:, 1:] & valid[:, :-1]
    rise = np.diff(z, axis=1)[pairs]
    return {
        "selected_segments": len(selected),
        "points": xy.size // 2,
        "outside_points": int((~valid).sum()),
        "covered_pairs": int(pairs.sum()),
        "rising_pairs_over_1e7_m": int((rise > 1e-7).sum()),
        "maximum_rise_m": float(rise.max(initial=0)),
    }


def compare(args):
    commit = frozen()
    values = inputs(args.input, args.spacing_m, args.weights)
    mesh, flow, rows, _source, weights, *_rest = values
    first = reference.fit_surface(mesh, flow, directed_rows=rows)
    if first.mesh is None:
        raise ValueError("unconstrained-pin LP reference failed")
    z = first.mesh.vertices[:, 2]
    pins = (
        {int(z.argmin()): float(z.min()), int(z.argmax()): float(z.max())}
        if args.pins
        else {}
    )
    observed, old_lp = {}, reference.linprog

    def linear_program(*positional, **named):
        observed["lp"] = named
        return old_lp(*positional, **named)

    with patch.object(reference, "linprog", side_effect=linear_program):
        lp = reference.fit_surface(mesh, flow, directed_rows=rows, fixed=pins)
    old_qp = weighted.clarabel.DefaultSolver

    def quadratic_program(p, q, a, b, cones, settings):
        observed["qp"] = (p, q, a, b)
        return old_qp(p, q, a, b, cones, settings)

    with patch.object(
        weighted.clarabel, "DefaultSolver", side_effect=quadratic_program
    ):
        qp = weighted.fit_weighted_surface(
            mesh, flow, weights=weights, directed_rows=rows, fixed=pins
        )
    if lp.mesh is None or qp.mesh is None:
        raise ValueError(
            "same-constraint solve failed: " + str((dict(lp.report), dict(qp.report)))
        )
    a_lp = observed["lp"]["A_ub"][2 * len(mesh.vertices) :, :-1]
    b_lp = observed["lp"]["b_ub"][2 * len(mesh.vertices) :]
    _p, _q, a_qp, b_qp = observed["qp"]
    difference = a_lp - a_qp[: len(b_lp)]
    difference.eliminate_zeros()
    if difference.nnz or not np.array_equal(b_lp, b_qp[: len(b_lp)]):
        raise ValueError("LP and QP gradient constraints differ")
    contact = observed["lp"]["A_eq"]
    if pins and (contact[:, :-1] - a_qp[len(b_lp) :]).nnz:
        raise ValueError("LP and QP contact equations differ")
    if pins and not np.array_equal(observed["lp"]["b_eq"], b_qp[len(b_lp) :]):
        raise ValueError("LP and QP hard heights differ")
    return commit, values, pins, lp, qp, observed


def run(args):
    started = time.perf_counter()
    if not args.output.resolve().is_relative_to(Path("/tmp")) or args.output.exists():
        raise ValueError("fresh explicit /tmp output required")
    commit, values, pins, lp, qp, observed = compare(args)
    mesh, flow, rows, source, weights, topology, coverage, segments, levels = values
    args.output.mkdir(parents=True)
    scores, artifacts = {}, {}
    for name, fit in (("minimax", lp), ("weighted", qp)):
        residual = fit.mesh.vertices[:, 2] - mesh.vertices[:, 2]
        scores[name] = {
            "fit": dict(fit.report),
            "weighted_rms_m": float(np.sqrt(np.dot(weights, residual**2))),
            "maximum_error_m": float(np.abs(residual).max()),
            "source_path": path_check(fit.mesh, segments, levels),
        }
        if scores[name]["source_path"]["rising_pairs_over_1e7_m"]:
            raise ValueError("whole source path has an unreported rise")
        path = args.output / (name + ".npz")
        np.savez_compressed(
            path,
            vertices=fit.mesh.vertices,
            triangles=mesh.triangles,
            prior=mesh.vertices[:, 2],
            flow=flow,
            directed_rows=rows,
            directed_source_segment=source,
            objective_weights=weights,
        )
        artifacts[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    p, q, a, b = observed["qp"]
    problem = args.output / "problem.npz"
    np.savez_compressed(
        problem,
        hessian_diagonal=p.diagonal(),
        linear=q,
        matrix_data=a.data,
        matrix_indices=a.indices,
        matrix_indptr=a.indptr,
        matrix_shape=a.shape,
        rhs=b,
        inequality_count=len(b) - len(pins),
        reference_height_m=np.median(mesh.vertices[:, 2]),
        fixed_indices=list(pins),
        fixed_heights=list(pins.values()),
    )
    artifacts[problem.name] = hashlib.sha256(problem.read_bytes()).hexdigest()
    report = {
        "commit": commit,
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "provenance_sha256": hashlib.sha256(
            args.input.with_suffix(".json").read_bytes()
        ).hexdigest(),
        "vertices": len(mesh.vertices),
        "faces": len(mesh.triangles),
        "spacing_m": args.spacing_m,
        "weights": args.weights,
        "fixed_heights": pins,
        "topology": topology,
        "source_path_coverage": coverage,
        "scores": scores,
        "artifacts": artifacts,
        "identical_directed_constraint_coefficients_bounds_and_pins": True,
        "pipeline_ms": (time.perf_counter() - started) * 1000,
        "scope": "Same finite directional constraints, different explicitly scored objectives. Directions and optional LP-derived pins are hypotheses, not observed OSM flow or water contacts. No shared fitter/native admission.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--spacing-m", type=float, default=20.0)
    parser.add_argument("--weights", choices=("area", "uniform"), default="area")
    parser.add_argument("--pins", action="store_true")
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3,) * 2)
    run(parser.parse_args())
