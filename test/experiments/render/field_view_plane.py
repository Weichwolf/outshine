"""One physical support plane, independent of the unchanged capture coordinates."""

import math
from dataclasses import dataclass

import numpy as np
from field_view_projection import vector


@dataclass(frozen=True)
class FieldPlane:
    """Normalized depth h/H = a*u+b*v+c along the capture direction.

    u/v are coordinates along capture right/up, divided by H. Zero coefficients
    retain the original capture-facing support. This has no depth authority.
    """

    coefficients: tuple = (0.0, 0.0, 0.0)

    def __post_init__(self):
        object.__setattr__(self, "coefficients", vector(self.coefficients))

    def normal(self, grid):
        a, b, _ = self.coefficients
        scale = math.hypot(1, a, b)
        right, up = grid.axes()
        return (
            np.asarray(grid.toward_eye) / scale - (a / scale) * right - (b / scale) * up
        )

    def centre(self, grid):
        return np.asarray(grid.centre) + (
            self.coefficients[2] * grid.half_extent_m
        ) * np.asarray(grid.toward_eye)

    def points(self, grid, coordinates):
        right, up = grid.axes()
        a, b, c = self.coefficients
        height = a * coordinates[:, 0] + b * coordinates[:, 1] + c
        return np.asarray(grid.centre) + grid.half_extent_m * (
            coordinates[:, :1] * right
            + coordinates[:, 1:] * up
            + height[:, None] * grid.toward_eye
        )


def fit_plane(depth, mask):
    """Fit all occupied first-hit depths, never RGB or a selected query pose."""
    if (
        not isinstance(depth, np.ndarray)
        or not isinstance(mask, np.ndarray)
        or mask.dtype != bool
        or mask.ndim != 2
        or mask.shape != depth.shape
        or mask.shape[0] != mask.shape[1]
        or not 3 <= len(mask) <= 512
        or not np.isfinite(depth).all()
        or np.any((depth < 0) | (depth > 1))
    ):
        raise ValueError(
            "Bounded square finite capture depth and explicit mask required"
        )
    count = int(np.count_nonzero(mask))
    if count < 3:
        raise ValueError("At least three occupied plane-fit points required")
    y, x = np.nonzero(mask)
    design = np.column_stack(
        ((x + 0.5) / len(mask) * 2 - 1, 1 - (y + 0.5) / len(mask) * 2, np.ones(count))
    )
    heights = 4 * depth[mask].astype(float) - 2
    coefficients, _, rank, singular = np.linalg.lstsq(design, heights, rcond=None)
    if rank != 3 or not np.isfinite(coefficients).all():
        raise ValueError("Occupied source points do not identify one finite plane")
    residual = heights - design @ coefficients
    return FieldPlane(tuple(coefficients)), {
        "source_points": count,
        "parameters": 3,
        "rank": int(rank),
        "condition_number": float(singular[0] / singular[-1]),
        "design_matrix_bytes": design.nbytes,
        "height_array_bytes": heights.nbytes,
        "normalized_depth_rms": float(np.sqrt(np.mean(residual**2))),
        "normalized_depth_maximum_residual": float(abs(residual).max()),
        "scope": "Unweighted three-parameter least squares of every occupied first-hit depth. No colour/image fit, original-hull certificate, native solver/runtime or pose/World admission.",
    }
