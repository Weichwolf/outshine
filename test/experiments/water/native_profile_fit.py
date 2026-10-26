"""Run the native HiGHS probe and independently inspect the unchanged finite problem."""

import json
import subprocess
import time
from pathlib import Path

import numpy as np
from directional_river_fit import face_gradients
from native_highs_model import write_model
from outshine_ref.water._mass import area_mass
from river_linear_model import linear_system
from scipy import sparse
from weighted_river_fit import directions as normalize_directions


def area_weights(mesh):
    triangle = mesh.vertices[mesh.triangles, :2]
    u, v = triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]
    area = np.abs(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]) / 2
    weights = np.bincount(
        mesh.triangles.ravel(), np.repeat(area / 3, 3), minlength=len(mesh.vertices)
    )
    if np.any(weights <= 0) or not np.isfinite(weights).all():
        raise ValueError("positive finite lumped vertex areas required")
    return weights / weights.mean()


def inspect_solution(model, values, column_dual, row_dual):
    matrix, bound, cost, hessian, lower, upper = model
    activity = matrix @ values
    residual = cost + hessian @ values - matrix.T @ row_dual - column_dual
    lower_known, upper_known = np.isfinite(lower), np.isfinite(upper)
    violation = max(
        float((activity - bound).max(initial=0)),
        float((lower[lower_known] - values[lower_known]).max(initial=0)),
        float((values[upper_known] - upper[upper_known]).max(initial=0)),
    )
    dual_sign = max(
        float(row_dual.max(initial=0)),
        float(column_dual[~lower_known].max(initial=0)),
        float((-column_dual[~upper_known]).max(initial=0)),
    )
    complementarity = [np.abs(row_dual * (bound - activity))]
    for selected, multiplier, gap in (
        (
            lower_known,
            np.maximum(column_dual, 0),
            values - np.where(lower_known, lower, 0),
        ),
        (
            upper_known,
            np.maximum(-column_dual, 0),
            np.where(upper_known, upper, 0) - values,
        ),
    ):
        complementarity.append(np.abs(multiplier[selected] * gap[selected]))
    return {
        "primal_violation": violation,
        "dual_sign_violation": dual_sign,
        "stationarity_residual": float(np.max(np.abs(residual), initial=0)),
        "complementarity_residual": float(
            np.max(np.concatenate(complementarity), initial=0)
        ),
    }


def run_native(
    binary,
    output,
    mesh,
    flow,
    rows,
    objective,
    *,
    fixed=None,
    down_grade=0.1,
    cross_grade=0.005,
    seconds=2,
    iterations=20000,
):
    began = time.perf_counter()
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    flow, _ = normalize_directions(mesh, flow, rows)
    matrix, bound, datum, target = linear_system(
        mesh, flow, rows, down_grade, cross_grade
    )
    nv = len(target)
    area = area_weights(mesh)
    if objective == "linf":
        cost, hessian, offset = np.r_[np.zeros(nv), 1.0], None, 0
    elif objective in ("l2-uniform", "l2-lumped", "l2-area"):
        matrix, bound = matrix[2 * nv :, :nv].tocsr(), bound[2 * nv :]
        hessian = (
            area_mass(mesh)
            if objective == "l2-area"
            else sparse.diags(np.ones(nv) if objective == "l2-uniform" else area)
        )
        cost, offset = -hessian @ target, float(0.5 * target @ hessian @ target)
    else:
        raise ValueError("known objective required")
    lower, upper = np.full(len(cost), -np.inf), np.full(len(cost), np.inf)
    if objective == "linf":
        lower[-1] = 0
    pins = dict(fixed or {})
    if any(
        type(i) is not int or not 0 <= i < nv or not np.isfinite(h)
        for i, h in pins.items()
    ):
        raise ValueError("finite hard heights at existing vertices required")
    for index, height in pins.items():
        lower[index] = upper[index] = height - datum
    write_model(
        output / "model.mps", matrix, cost, lower, upper, bound, hessian, offset
    )
    export_ms = (time.perf_counter() - began) * 1000
    result = subprocess.run(
        [
            str(binary),
            str(output / "model.mps"),
            str(output / "native.json"),
            str(seconds),
            str(iterations),
        ],
        capture_output=True,
        text=True,
        timeout=max(seconds, 0) + 15,
        check=False,
    )
    (output / "process.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f"native probe exited {result.returncode}: {result.stderr}")
    native = json.loads((output / "native.json").read_text())
    report = {
        k: v
        for k, v in native.items()
        if k not in ("values", "column_dual", "row_dual")
    }
    report.update(
        valid=False,
        objective=objective,
        export_ms=export_ms,
        matrix_nonzeros=matrix.nnz,
        vertices=nv,
        faces=len(mesh.triangles),
    )
    if not native["optimal"]:
        report.update(
            reason="no_optimal_native_solution",
            pipeline_ms=(time.perf_counter() - began) * 1000,
        )
        return None, report
    values = np.asarray(native["values"])
    column_dual, row_dual = (
        np.asarray(native["column_dual"]),
        np.asarray(native["row_dual"]),
    )
    if (
        values.shape != cost.shape
        or column_dual.shape != cost.shape
        or row_dual.shape != bound.shape
        or not all(np.isfinite(a).all() for a in (values, column_dual, row_dual))
    ):
        raise RuntimeError("native primal/dual dimensions or numbers are invalid")
    certificate = inspect_solution(
        (
            matrix,
            bound,
            cost,
            sparse.csr_matrix((len(cost), len(cost))) if hessian is None else hessian,
            lower,
            upper,
        ),
        values,
        column_dual,
        row_dual,
    )
    height = values[:nv] + datum
    gradients = face_gradients(mesh, height)
    cross = np.c_[-flow[:, 1], flow[:, 0]]
    along_grade = np.einsum("fi,fi->f", gradients, flow)
    cross_grade_actual = np.einsum("fi,fi->f", gradients, cross)
    directions = rows[:, 1:3] / np.linalg.norm(rows[:, 1:3], axis=1)[:, None]
    directed = np.einsum("fi,fi->f", gradients[rows[:, 0].astype(int)], directions)
    grade_error = max(
        float(along_grade.max()),
        float((-along_grade - down_grade).max()),
        float((np.abs(cross_grade_actual) - cross_grade).max()),
        float((directed - rows[:, 4]).max(initial=0)),
        float((rows[:, 3] - directed).max(initial=0)),
        0,
    )
    pin_error = max((abs(height[i] - h) for i, h in pins.items()), default=0)
    errors = height - mesh.vertices[:, 2]
    mass = area_mass(mesh)
    report.update(
        certificate,
        maximum_grade_violation_m_per_m=grade_error,
        fixed_height_error_m=float(pin_error),
        maximum_prior_error_m=float(np.max(np.abs(errors))),
        uniform_rms_error_m=float(np.sqrt(np.mean(errors**2))),
        lumped_area_rms_error_m=float(np.sqrt(np.average(errors**2, weights=area))),
        integrated_rms_error_m=float(np.sqrt((errors @ mass @ errors) / mass.sum())),
    )
    if objective == "linf":
        report["objective_error_m"] = float(values[-1])
    valid = bool(
        max(certificate.values()) <= 1e-6 and grade_error <= 1e-7 and pin_error <= 1e-7
    )
    report.update(
        valid=valid,
        reason="" if valid else "independent_native_check_failed",
        pipeline_ms=(time.perf_counter() - began) * 1000,
    )
    return height if report["valid"] else None, report
