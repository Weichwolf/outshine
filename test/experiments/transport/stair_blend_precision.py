"""Float32 facet reserves from a declared height interval and eight rounding corners."""

import itertools

import clarabel
import numpy as np
from outshine_ref.transport.heights import _gradient_weights
from scipy import sparse

SOIL = 2 / 3
RESERVE = 0.002
EXPORT_RESERVE = 1e-6


def policy(layout):
    return str(np.asarray(layout.get("precision_mode", "legacy")).item())


def height_interval(layout):
    fixed = layout["bed"][np.isfinite(layout["bed"])]
    values = np.r_[layout["targets"], fixed]
    return float(values.min() - 8), float(values.max() + 8)


def guard_data(layout, faces, eligible, structure_limit):
    xy = layout["xy"]
    quantized = xy.astype(np.float32).astype(float)
    coefficients = _gradient_weights(quantized, faces)
    points = xy[faces]
    transforms = coefficients @ (points - points[:, :1])
    stretch = np.linalg.svd(transforms, compute_uv=False)[:, 0]
    low, high = height_interval(layout)
    epsilon = float(abs(np.spacing(np.float32(max(abs(low), abs(high)))))) / 2
    signs = np.array(list(itertools.product((-1, 1), repeat=3)))
    error = epsilon * np.linalg.norm(coefficients @ signs.T, axis=1).max(axis=1)
    error = np.nextafter(error, np.inf)
    risk = stretch * (SOIL - RESERVE) + error + EXPORT_RESERVE - SOIL
    support_risk = (
        stretch * (structure_limit - RESERVE) + error + EXPORT_RESERVE - structure_limit
    )
    risk = np.where(eligible, np.maximum(risk, support_risk), risk)
    selected = (
        np.arange(len(faces)) if policy(layout) == "all" else np.flatnonzero(risk > 0)
    )
    return coefficients, selected, error, (low, high), epsilon


def blocks(layout, faces, eligible, structure_limit, datum, gradient_matrix):
    mode = policy(layout)
    if mode == "legacy":
        return [], [], [], {}
    if mode not in ("selective", "all"):
        raise ValueError("known explicit terrain precision policy required")
    coefficients, selected, error, interval, epsilon = guard_data(
        layout, faces, eligible, structure_limit
    )
    count = len(layout["xy"])
    eligible_ids = np.flatnonzero(eligible)
    total = count + len(eligible_ids)
    cone = np.concatenate(
        [np.zeros((len(selected), 1, 3)), -coefficients[selected]], axis=1
    )
    matrix = gradient_matrix(faces[selected], cone, total)
    positions = np.full(len(faces), -1, int)
    positions[eligible_ids] = np.arange(count, total)
    allowance_rows = np.flatnonzero(eligible[selected])
    matrix += sparse.csc_matrix(
        (
            -np.ones(len(allowance_rows)),
            (3 * allowance_rows, positions[selected[allowance_rows]]),
        ),
        shape=matrix.shape,
    )
    rhs = np.c_[
        SOIL - EXPORT_RESERVE - error[selected], np.zeros((len(selected), 2))
    ].ravel()
    identity = sparse.eye(count, total, format="csc")
    matrices = [matrix, identity, -identity]
    bounds = [
        rhs,
        np.full(count, interval[1] - datum),
        np.full(count, datum - interval[0]),
    ]
    cones = [clarabel.SecondOrderConeT(3) for _ in selected]
    cones += [clarabel.NonnegativeConeT(count), clarabel.NonnegativeConeT(count)]
    note = {
        "mode": mode,
        "physical_height_interval_m": list(interval),
        "height_rounding_bound_m": epsilon,
        "export_grade_reserve": EXPORT_RESERVE,
        "evaluated_facets": len(faces),
        "rounding_corners_per_facet": 8,
        "additional_gradient_cones": len(selected),
        "additional_height_rows": 2 * count,
        "explicit_xy_quantization_copy_bytes": count * 24,
        "maximum_gradient_rounding_bound": float(error.max(initial=0)),
        "scope": "Own sufficient reserves, fixed height interval (target/pin extrema plus 8m), actual rounded XY, corner extrema and affine operator stretch; final actual export admission remains mandatory.",
    }
    return matrices, bounds, cones, note


def check_interval(layout, heights):
    if policy(layout) != "legacy":
        low, high = height_interval(layout)
        if heights.min() < low or heights.max() > high:
            raise ValueError(
                "actual terrain escapes its declared precision height interval"
            )
