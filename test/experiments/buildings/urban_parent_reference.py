"""Adapt existing resolved fields to the shared DDA; only an offline control."""

import itertools

import numpy as np
from column_field_query import trace


def prepare_resolved(product):
    """The adapter constructs at most one bottom-zero prism per indexed cell.

    column_geometry records that construction contract, not World readiness or
    completeness outside the camera domain explicitly declared by a consumer.
    """
    first = np.zeros(2, np.int64)
    size = np.zeros(2, np.int64)
    if len(product.chunks):
        first = product.chunks.min(axis=0).astype(np.int64) * product.side
        size = (product.chunks.max(axis=0).astype(np.int64) + 1) * product.side - first
    if np.prod(size) > 4194304:
        raise ValueError("bounded offline resolved field required")
    grid = np.zeros(tuple(size[::-1]), float)
    for chunk, key in enumerate(product.chunks):
        x, y = key * product.side - first
        grid[y : y + product.side, x : x + product.side] = product.heights[
            product.tiers[chunk]
        ]
    y, x = np.nonzero(grid)
    lookup = np.full(grid.shape, -1, np.int32)
    lookup[y, x] = np.arange(len(x), dtype=np.int32)
    spans = np.column_stack((np.zeros(len(x)), grid[y, x]))
    result = {
        "columns": np.column_stack((x, y)).astype(np.int32),
        "spans": spans,
        "offsets": np.arange(len(x) + 1, dtype=np.uint32),
        "lookup": lookup,
        "low": np.zeros(2, np.int64),
        "shape": size,
        "basis": np.eye(3),
        "column_geometry": "single_grounded_prism",
        "height_bounds": np.array([0, product.heights.max()]),
        "header": {
            "pitch_m": product.pitch,
            "footprint_fraction": float(product.frame[4]),
        },
        "reference_origin_m": np.r_[
            (first * product.frame[3] - product.frame[:2]) * product.frame[2], 0
        ],
        "scope": "Existing resolved fields only as offline opaque reference; the parent compiler never calls this adapter.",
    }
    result["array_bytes"] = sum(
        value.nbytes for value in result.values() if isinstance(value, np.ndarray)
    )
    return result


def trace_resolved(product, origins, directions, *, minimum=0.1, inside_is_hit=False):
    return trace(
        product,
        np.asarray(origins) - product["reference_origin_m"],
        directions,
        minimum=minimum,
        inside_is_hit=inside_is_hit,
    )


def support_points(product, height, profile):
    low = product["reference_origin_m"][:2]
    high = low + product["shape"] * product["header"]["pitch_m"]
    corners = np.array(list(itertools.product(*zip(low, high, strict=True))))
    if profile["kind"] == "point":
        light = np.asarray(profile["position"], float)
        if light[2] <= height:
            raise ValueError("finite flat-base support requires light above model")
        shadows = corners + (corners - light[:2]) * height / (light[2] - height)
    else:
        light = np.asarray(profile["direction"], float)
        if light[2] <= 0:
            raise ValueError("finite flat-base support requires positive light z")
        shadows = corners - height * light[:2] / light[2]
    return np.vstack(
        (
            np.c_[corners, np.zeros(4)],
            np.c_[corners, np.full(4, height)],
            np.c_[shadows, np.zeros(4)],
        )
    )


def projected_region(parent, resolved, view, profile):
    """Diagnostic support of this flat inferred model, never World visibility."""
    from triangle_image_reference import projected_bounds
    from urban_parent_conditional import grid

    products = [grid(parent), resolved]
    points = [
        support_points(p, parent.heights[-1], profile)
        for p in products
        if np.all(p["shape"] > 0)
    ]
    box = projected_bounds(np.vstack(points), view)
    return np.array(
        [
            np.maximum(np.floor(box[0]), [0, 0]),
            np.minimum(np.ceil(box[1]), [view[3], view[4]]),
        ],
        int,
    )
