"""Coverage-only comparison streams alpha; shading still needs depth and normal data."""

import numpy as np
from leaf_sheet import average_blocks, sample_image
from raster_batch import rasterize
from shoot_plan import normal_frame


def coverage(source, charts, direction, bounds, pixels=8):
    if type(pixels) is not int or pixels not in (4, 8, 16):
        raise ValueError("declared finite coverage footprint required")
    basis = normal_frame(np.asarray(direction, float), np.array((0, 0, 1)))
    frame = np.column_stack((basis[:, 0], basis[:, 2], basis[:, 1]))
    lower, upper = bounds
    size = pixels * 8
    mask, _ = rasterize(
        source["vertices"] @ frame,
        source["faces"],
        np.empty((len(source["faces"]), 3, 0)),
        lower,
        upper,
        size,
    )
    transmission = np.ones((size, size))
    for chart in charts:
        mesh = chart.mesh
        covered, uv = rasterize(
            mesh["vertices"] @ frame,
            mesh["faces"],
            mesh["uv"][mesh["faces"]],
            lower,
            upper,
            size,
        )
        alpha = sample_image(np.rint(chart.normal[..., 3, None] * 255) / 255, uv)[
            ..., 0
        ]
        transmission *= 1 - alpha * covered
    reference = average_blocks(mask.astype(float), 8)
    reduced = average_blocks(1 - transmission, 8)
    union = np.maximum(reference, reduced).sum()
    return (
        {
            "coverage_l1_over_union": float(
                abs(reference - reduced).sum() / max(union, 1e-12)
            ),
            "area_relative_error": float(
                abs(reference.sum() - reduced.sum()) / max(reference.sum(), 1e-12)
            ),
        },
        reference,
        reduced,
    )
