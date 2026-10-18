"""Camera-independent two-level terrain cells; constrained contacts stay fine."""

import numpy as np
import shapely

COARSE_CONTROL_ERROR_M = 0.25


def dem_errors(cells, sample):
    bounds = shapely.bounds(cells)
    control = np.array(
        [
            [0, 0],
            [1, 0],
            [1, 1],
            [0, 1],
            [0.5, 0],
            [1, 0.5],
            [0.5, 1],
            [0, 0.5],
            [0.5, 0.5],
        ]
    )
    xy = bounds[:, None, :2] + control * (bounds[:, None, 2:] - bounds[:, None, :2])
    values = np.asarray(sample(xy.reshape(-1, 2)), float)
    if values.shape != (len(cells) * 9,) or not np.isfinite(values).all():
        raise ValueError("actual finite DEM controls required before coarsening")
    values = values.reshape(-1, 9)
    corners = values[:, :4]
    edges = (corners + np.roll(corners, -1, axis=1)) / 2
    error = abs(values[:, 4:8] - edges).max(axis=1)
    for a, b in ((0, 2), (1, 3)):
        error = np.maximum(
            error, abs(values[:, 8] - (corners[:, a] + corners[:, b]) / 2)
        )
    return error


def transport_cells(xs, ys, transport, *, near_m, sample=None):
    nx, ny = len(xs) - 1, len(ys) - 1
    if nx < 1 or ny < 1 or nx * ny > 16384:
        raise ValueError("bounded original terrain cell domain required")
    xs, ys = np.asarray(xs, float), np.asarray(ys, float)
    if (
        xs.ndim != 1
        or ys.ndim != 1
        or not np.isfinite(xs).all()
        or not np.isfinite(ys).all()
        or np.any(np.diff(xs) <= 0)
        or np.any(np.diff(ys) <= 0)
        or max(np.diff(xs).max(), np.diff(ys).max()) > 12 + 1e-9
        or not np.isfinite(near_m)
        or near_m < 4 * max(np.diff(xs).max(), np.diff(ys).max())
    ):
        raise ValueError(
            "finite fine axes up to twelve metres and full near band required"
        )
    if (
        not isinstance(transport, shapely.Geometry)
        or transport.is_empty
        or not transport.is_valid
    ):
        raise ValueError("actual valid transport projection required")
    blocks = [
        (i, min(i + 2, nx), j, min(j + 2, ny))
        for j in range(0, ny, 2)
        for i in range(0, nx, 2)
    ]
    coarse = shapely.box(
        [xs[i] for i, _a, j, _b in blocks],
        [ys[j] for i, _a, j, _b in blocks],
        [xs[a] for _i, a, _j, b in blocks],
        [ys[b] for _i, a, _j, b in blocks],
    )
    fine = shapely.intersects(coarse, transport.buffer(near_m))
    tested = np.flatnonzero(~fine)
    errors = np.empty(0)
    refined = 0
    if sample is None:
        fine[:] = True
    elif len(tested):
        errors = dem_errors(coarse[tested], sample)
        refined = int(np.sum(errors > COARSE_CONTROL_ERROR_M))
        fine[tested[errors > COARSE_CONTROL_ERROR_M]] = True
    cells = []
    for (i, a, j, b), shape, keep in zip(blocks, coarse, fine, strict=True):
        if keep:
            cells.extend(
                shapely.box(xs[x], ys[y], xs[x + 1], ys[y + 1])
                for y in range(j, b)
                for x in range(i, a)
            )
        else:
            cells.append(shape)
    return np.array(cells, dtype=object), {
        "sampling": "transport_neighborhood",
        "base_cells": nx * ny,
        "retained_cells": len(cells),
        "coarse_blocks": int((~fine).sum()),
        "near_transport_m": near_m,
        "maximum_far_cell_width_m": 2 * max(np.diff(xs).max(), np.diff(ys).max()),
        "dem_control_samples": len(tested) * 9 if sample is not None else 0,
        "dem_refined_blocks": refined,
        "coarse_control_error_limit_m": COARSE_CONTROL_ERROR_M,
        "maximum_candidate_control_error_m": float(errors.max(initial=0)),
        "maximum_retained_control_error_m": float(
            errors[errors <= COARSE_CONTROL_ERROR_M].max(initial=0)
        ),
        "scope": "Same complete rectangle; fine cells near actual transport and where coarse midpoint errors exceed 0.25m on the original fine lattice (both diagonals). No whole continuous DEM/fitted-field error bound. Whole topology/physical admission remains mandatory.",
    }
