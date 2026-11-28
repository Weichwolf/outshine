"""Continuous box integrals of coupled linear premultiplied radiance/coverage.

The zero-padded exclusive prefix is bilinear within each source cell. Four such
evaluations integrate the piecewise-constant source exactly in real arithmetic.
Float32 and Float64 are separate measured execution profiles; no normal/material
averaging, perspective footprint approximation or depth admission occurs here.
"""

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class SummedField:
    table: object
    source_width: int
    source_height: int


def summed_field(radiance, *, dtype=np.float32, texels_most=262144):
    if (
        not isinstance(radiance, np.ndarray)
        or radiance.ndim != 3
        or radiance.shape[2] != 4
        or min(radiance.shape[:2]) < 1
        or radiance.dtype.kind != "f"
    ):
        raise ValueError("A nonempty floating-point coupled RGBA image is required")
    if type(texels_most) is not int or texels_most < 1:
        raise ValueError("A positive source-texel capacity is required")
    height, width = radiance.shape[:2]
    if height * width > texels_most:
        raise ValueError("Source-texel capacity exceeded before prefix allocation")
    dtype = np.dtype(dtype)
    if dtype not in (np.dtype(np.float32), np.dtype(np.float64)):
        raise ValueError("Only explicit Float32/Float64 prefix profiles are supported")
    if not np.isfinite(radiance).all() or np.any(radiance < 0):
        raise ValueError("Finite nonnegative linear premultiplied radiance required")
    alpha = radiance[:, :, 3]
    if np.any(alpha > 1) or np.any(radiance[:, :, :3][alpha == 0] != 0):
        raise ValueError("Coverage must lie in[0,1], with transparent black outside")
    with np.errstate(over="ignore", invalid="ignore"):
        table = np.pad(
            radiance.cumsum(axis=0, dtype=dtype).cumsum(axis=1, dtype=dtype),
            ((1, 0), (1, 0), (0, 0)),
        )
    if not np.isfinite(table).all():
        raise ValueError("Prefix profile overflow")
    owned = np.frombuffer(table.tobytes(), dtype=dtype).reshape(table.shape)
    return SummedField(owned, width, height)


def prefix_at(field, coordinates):
    table = field.table
    xy = coordinates.astype(table.dtype, copy=False)
    x = np.clip(xy[:, 0], 0, field.source_width)
    y = np.clip(xy[:, 1], 0, field.source_height)
    i = np.minimum(np.floor(x).astype(np.int64), field.source_width - 1)
    j = np.minimum(np.floor(y).astype(np.int64), field.source_height - 1)
    dx, dy = (x - i).astype(table.dtype)[:, None], (y - j).astype(table.dtype)[:, None]
    return (
        (1 - dx) * (1 - dy) * table[j, i]
        + dx * (1 - dy) * table[j, i + 1]
        + (1 - dx) * dy * table[j + 1, i]
        + dx * dy * table[j + 1, i + 1]
    )


def box_integrals(field, lower, upper, *, boxes_most=16384):
    if (
        not isinstance(lower, np.ndarray)
        or not isinstance(upper, np.ndarray)
        or lower.ndim != 2
        or lower.shape[1] != 2
        or upper.shape != lower.shape
        or lower.dtype.kind != "f"
        or upper.dtype.kind != "f"
    ):
        raise ValueError("Matching floating-point Nx2 box coordinates are required")
    if type(boxes_most) is not int or boxes_most < 1 or len(lower) > boxes_most:
        raise ValueError("Bounded positive rectangle-lookup capacity required")
    if (
        not np.isfinite(lower).all()
        or not np.isfinite(upper).all()
        or np.any(upper < lower)
    ):
        raise ValueError("Ordered finite source-cell boxes required")
    result = prefix_at(field, upper) + prefix_at(field, lower)
    result -= prefix_at(field, np.column_stack((lower[:, 0], upper[:, 1])))
    result -= prefix_at(field, np.column_stack((upper[:, 0], lower[:, 1])))
    return result
