"""Compare identical whole-face/contact constraints with two DEM objectives."""

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import (
    HeightBudget,
    fit_surface_heights,
)
from outshine_ref.transport.weighted_heights import fit_weighted_heights


def audit(xy, faces, targets, frames, heights, fixed, datum, weights):
    vertices = np.c_[xy, heights][faces]
    normal = np.cross(vertices[:, 1] - vertices[:, 0], vertices[:, 2] - vertices[:, 0])
    gradient = -normal[:, :2] / normal[:, 2, None]
    normalized = np.linalg.norm(np.einsum("fij,fj->fi", frames, gradient), axis=1)
    error = abs(heights - targets)
    pins = abs(heights[fixed] - datum)
    return {
        "valid": bool(
            np.isfinite(heights).all()
            and normalized.max() <= 1 + 1e-7
            and pins.max(initial=0) <= 1e-7
        ),
        "maximum_normalized_grade": float(normalized.max()),
        "maximum_pin_error_m": float(pins.max(initial=0)),
        "maximum_dem_vertex_error_m": float(error.max()),
        "weighted_dem_rms_m": float(np.sqrt(np.average(error**2, weights=weights))),
        "dem_vertex_rms_m": float(np.sqrt(np.mean(error**2))),
        "dem_vertex_p95_m": float(np.percentile(error, 95)),
    }


def fit(arrays, datum, mode, seconds=8):
    xy, faces, target, frames, fixed, weights = (
        arrays[k] for k in ("xy", "faces", "targets", "frames", "fixed", "weights")
    )
    if mode == "minimax":
        result = fit_surface_heights(
            Mesh(np.c_[xy, target], faces),
            frames,
            fixed={int(i): datum for i in fixed},
            budget=HeightBudget(
                time_limit_s=seconds, max_vertices=131072, max_faces=160000
            ),
        )
        heights = result.mesh.vertices[:, 2] if result.mesh else None
        report = dict(result.report)
    elif mode == "least_squares":
        result = fit_weighted_heights(
            Mesh(np.c_[xy, target], faces),
            frames,
            reference_datum_m=datum,
            weights=weights,
            fixed_vertices=fixed,
            budget=HeightBudget(
                time_limit_s=seconds, max_vertices=131072, max_faces=160000
            ),
        )
        heights = result.mesh.vertices[:, 2] if result.mesh else None
        report = dict(result.report)
    else:
        raise ValueError("known height objective required")
    report.update(valid=False, mode=mode, solver_time_limit_s=seconds)
    report.setdefault("reason", "no_valid_whole_surface")
    if heights is not None:
        check = audit(xy, faces, target, frames, heights, fixed, datum, weights)
        report["independent_check"] = check
        report["valid"] = check["valid"]
        if report["valid"]:
            report["reason"] = ""
    return heights if report["valid"] else None, report
