"""Bounded membrane repair, fixed road beds, and explicit constructed slope zones."""

import time

import clarabel
import numpy as np
import shapely
from outshine_ref.transport.heights import _gradient_weights
from scipy import sparse
from stair_blend_constraints import upper_rows
from stair_blend_precision import RESERVE, SOIL, check_interval
from stair_blend_precision import blocks as precision_blocks
from surface_height_checks import normal_gradients

SOIL_LIMIT = SOIL
REACH_M = 5.0


def pins_for(layout):
    xy, bed, target = (layout[k] for k in ("xy", "bed", "targets"))
    fixed, values = np.isfinite(bed), bed.copy()
    outer = np.any(
        np.isclose(xy, xy.min(axis=0), rtol=0, atol=1e-9)
        | np.isclose(xy, xy.max(axis=0), rtol=0, atol=1e-9),
        axis=1,
    )
    fixed |= outer
    values[outer] = target[outer]
    return np.flatnonzero(fixed), values


def gradient_matrix(faces, coefficients, count):
    rows = np.repeat(np.arange(coefficients.shape[0] * coefficients.shape[1]), 3)
    columns = np.broadcast_to(faces[:, None, :], coefficients.shape).ravel()
    return sparse.csc_matrix(
        (coefficients.ravel(), (rows, columns)),
        shape=(coefficients.shape[0] * coefficients.shape[1], count),
    )


def energy(xy, faces, target, smoothing, extra):
    coefficients = _gradient_weights(xy, faces)
    p = xy[faces]
    u, v = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    areas = abs(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]) / 2
    mass = np.full(len(xy), 0.001)
    np.add.at(mass, faces.ravel(), np.repeat(areas / 3, 3))
    matrix = sparse.diags(mass, format="csc")
    if smoothing:
        gradient = gradient_matrix(faces, coefficients, len(xy))
        stiffness = gradient.T @ sparse.diags(np.repeat(areas, 2)) @ gradient
        matrix = (matrix + smoothing * stiffness).tocsc()
    datum = float(np.median(target))
    linear = -2 * (matrix @ (target - datum))
    quadratic = sparse.block_diag(
        [2 * matrix, sparse.csc_matrix((extra, extra))]
    ).tocsc()
    return quadratic, linear, areas, coefficients, datum


def assemble(layout, upper, footprint, *, smoothing=16.0, structure_limit=10.0):
    if smoothing not in (0, 16) or structure_limit not in (2, 10):
        raise ValueError("explicit bounded terrain-repair variants required")
    xy, target = layout["xy"], layout["targets"]
    faces = layout["faces"][~layout["ground"]]
    if len(xy) > 16384 or len(layout["faces"]) > 32768:
        raise ValueError("terrain-repair input cap exhausted")
    eligible = shapely.covered_by(
        shapely.polygons(xy[faces]), footprint.buffer(REACH_M)
    )
    ids = np.flatnonzero(eligible)
    n, total = len(xy), len(xy) + len(ids)
    quadratic, linear, areas, coefficients, datum = energy(
        xy, faces, target, smoothing, len(ids)
    )
    objective = np.r_[linear, 100 * areas[eligible]]
    cone = np.concatenate([np.zeros((len(faces), 1, 3)), -coefficients], axis=1)
    gradient = gradient_matrix(faces, cone, total)
    gradient += sparse.csc_matrix(
        (-np.ones(len(ids)), (3 * ids, np.arange(n, total))), shape=gradient.shape
    )
    rhs = np.c_[
        np.full(len(faces), SOIL_LIMIT - RESERVE), np.zeros((len(faces), 2))
    ].ravel()
    cones = [clarabel.SecondOrderConeT(3) for _ in faces]
    pins, values = pins_for(layout)
    fixed = sparse.csc_matrix(
        (np.ones(len(pins)), (np.arange(len(pins)), pins)), shape=(len(pins), total)
    )
    allowance = sparse.csc_matrix(
        (np.ones(len(ids)), (np.arange(len(ids)), np.arange(n, total))),
        shape=(len(ids), total),
    )
    blocks = [gradient, fixed, -allowance, allowance]
    bounds = [
        rhs,
        values[pins] - datum,
        np.zeros(len(ids)),
        np.full(len(ids), structure_limit - SOIL_LIMIT),
    ]
    cones += [
        clarabel.ZeroConeT(len(pins)),
        clarabel.NonnegativeConeT(len(ids)),
        clarabel.NonnegativeConeT(len(ids)),
    ]
    extra, rhs_extra, cones_extra, precision_note = precision_blocks(
        layout, faces, eligible, structure_limit, datum, gradient_matrix
    )
    blocks.extend(extra)
    bounds.extend(rhs_extra)
    cones.extend(cones_extra)
    bridge = None
    bridge_note = {}
    if upper is not None:
        bridge, height, bridge_note = upper_rows(xy, faces, upper, footprint)
        blocks.append(
            sparse.hstack([bridge, sparse.csc_matrix((len(height), len(ids)))])
        )
        bounds.append(height - datum * np.asarray(bridge.sum(axis=1)).ravel())
        cones.append(clarabel.NonnegativeConeT(len(height)))
    return {
        "P": quadratic,
        "q": objective,
        "A": sparse.vstack(blocks).tocsc(),
        "b": np.concatenate(bounds),
        "cones": cones,
        "datum": datum,
        "pins": pins,
        "pin_values": values[pins],
        "eligible": eligible,
        "soil_faces": faces,
        "bridge": bridge,
        "bridge_note": bridge_note,
        "areas": areas,
        "structure_limit": structure_limit,
        "smoothing_m2": smoothing,
        "precision": precision_note,
    }


def field_grades(layout, heights, model):
    if not np.isfinite(heights).all():
        raise ValueError("nonfinite terrain field")
    faces, xy = model["soil_faces"], layout["xy"]
    first = np.linalg.norm(normal_gradients(xy, faces, heights), axis=1)
    actual = np.c_[xy, heights].astype(np.float32).astype(float)
    exported = np.linalg.norm(
        normal_gradients(actual[:, :2], faces, actual[:, 2]), axis=1
    )
    return first, exported


def check_grades(layout, heights, model, first, exported):
    check_interval(layout, heights)
    constructed = np.maximum(first, exported) > SOIL_LIMIT
    if np.any(constructed & ~model["eligible"]):
        raise ValueError("constructed slope escapes the declared contact zone")
    if np.any(
        np.maximum(first, exported)
        > np.where(constructed, model["structure_limit"], SOIL_LIMIT)
    ):
        raise ValueError("actual Double/Float32 terrain class grade exceeded")
    if not np.array_equal(heights[model["pins"]], model["pin_values"]):
        raise ValueError("whole terrain/road or outer contact changed")
    if model["bridge"] is not None and model["bridge"].shape[0]:
        rows = model["bridge"].shape[0]
        excess = model["bridge"] @ (heights - model["datum"]) - model["b"][-rows:]
        if excess.max() > 1e-7:
            raise ValueError("whole affine bridge bound violated")
    return constructed, {
        "maximum_soil_grade_double_float32": [
            float(q[~constructed].max(initial=0)) for q in (first, exported)
        ],
        "maximum_support_grade_double_float32": [
            float(q[constructed].max(initial=0)) for q in (first, exported)
        ],
        "constructed_faces": int(constructed.sum()),
        "constructed_projected_area_m2": float(model["areas"][constructed].sum()),
        "constructed_surface_area_m2": float(
            np.sum(model["areas"][constructed] * np.sqrt(1 + first[constructed] ** 2))
        ),
        "structural_reach_m": REACH_M,
    }


def check_heights(layout, heights, model):
    return check_grades(layout, heights, model, *field_grades(layout, heights, model))


def candidate(layout, upper, footprint, *, smoothing=16.0, structure_limit=10.0):
    began = time.perf_counter()
    model = assemble(
        layout, upper, footprint, smoothing=smoothing, structure_limit=structure_limit
    )
    settings = clarabel.DefaultSettings()
    settings.verbose = False
    settings.time_limit, settings.max_iter, settings.max_threads = 8, 100, 1
    settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-9
    solver = clarabel.DefaultSolver(
        model["P"], model["q"], model["A"], model["b"], model["cones"], settings
    )
    result = solver.solve()
    note = {
        "solver_status": str(result.status),
        "iterations": result.iterations,
        "solver_seconds": result.solve_time,
        "total_seconds": time.perf_counter() - began,
        "variables": len(model["q"]),
        "rows": len(model["b"]),
        "A_nonzeros": model["A"].nnz,
        "P_nonzeros": model["P"].nnz,
        "smoothing_m2": smoothing,
        "structure_limit": structure_limit,
        "bridge_bounds": model["bridge_note"],
        "precision": model["precision"],
        "retained_sparse_bytes": int(
            sum(
                matrix.data.nbytes + matrix.indices.nbytes + matrix.indptr.nbytes
                for matrix in (model["P"], model["A"])
            )
        ),
        "height_output_bytes": len(layout["xy"]) * 8,
        "explicit_height_conversion_and_datum_copy_bytes": len(layout["xy"]) * 16,
    }
    if str(result.status) not in ("Solved", "AlmostSolved", "MaxTime", "MaxIterations"):
        return None, model, note
    heights = np.asarray(result.x[: len(layout["xy"])]) + model["datum"]
    heights[model["pins"]] = model["pin_values"]
    return heights, model, note


def validate_candidate(layout, heights, model, note):
    if heights is None:
        note["accepted_field"] = False
        return None
    first, exported = field_grades(layout, heights, model)
    note["validation_xyz_conversion_copy_bytes"] = len(layout["xy"]) * 3 * 20
    note["maximum_candidate_grade_double_float32"] = [
        float(first.max()),
        float(exported.max()),
    ]
    try:
        constructed, proof = check_grades(layout, heights, model, first, exported)
    except ValueError as error:
        note.update(accepted_field=False, rejection=str(error))
        return None
    note.update(proof, accepted_field=True)
    return constructed


def solve(layout, upper, footprint, *, smoothing=16.0, structure_limit=10.0):
    heights, model, note = candidate(
        layout, upper, footprint, smoothing=smoothing, structure_limit=structure_limit
    )
    constructed = validate_candidate(layout, heights, model, note)
    return (heights if note["accepted_field"] else None), constructed, note
