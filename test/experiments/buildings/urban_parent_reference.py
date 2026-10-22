"""Adapt existing resolved fields to the shared DDA; only an offline control."""

import numpy as np
from column_field_query import trace


def prepare_resolved(product):
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


def trace_resolved(product, origins, directions, *, minimum=0.1):
    return trace(
        product,
        np.asarray(origins) - product["reference_origin_m"],
        directions,
        minimum=minimum,
    )
