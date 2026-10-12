"""Dense and conservatively culled scalar grids with identical crossing-cell values."""

import numpy as np
from fork_intervals import block_intervals


def grid_parameters(field, cells):
    if not isinstance(cells, int) or not 16 <= cells <= 192:
        raise ValueError("16 through 192 grid cells on the longest axis required")
    lower, upper = field.bounds()
    step = float(np.max(upper - lower) / cells)
    lower = lower - 2 * step
    shape = np.ceil((upper + 2 * step - lower) / step).astype(int) + 1
    if int(np.prod(shape)) > 8_000_000:
        raise ValueError("bounded grid volume required")
    return lower, step, tuple(int(v) for v in shape)


def evaluate_nodes(field, lower, step, shape, values, indices):
    for start in range(0, len(indices), 16384):
        selected = indices[start : start + 16384]
        coordinates = np.column_stack(np.unravel_index(selected, shape))
        values[selected] = field.evaluate(lower + coordinates * step)


def dense_volume(field, cells):
    lower, step, shape = grid_parameters(field, cells)
    count = int(np.prod(shape))
    values = np.empty(count, dtype=np.float32)
    evaluate_nodes(field, lower, step, shape, values, np.arange(count))
    return (
        values.reshape(shape),
        lower,
        step,
        {
            "field_point_evaluations": count,
            "interval_block_evaluations": 0,
            "classification_mask_bytes": 0,
        },
    )


def blocked_volume(field, cells, block_size=4):
    if not isinstance(block_size, int) or not 2 <= block_size <= 16:
        raise ValueError("two through 16 grid cells per block required")
    lower, step, shape = grid_parameters(field, cells)
    starts = np.stack(
        np.meshgrid(*(np.arange(0, v - 1, block_size) for v in shape), indexing="ij"),
        axis=-1,
    ).reshape(-1, 3)
    ends = np.minimum(starts + block_size, np.asarray(shape) - 1)
    lows, highs = block_intervals(field, lower + starts * step, lower + ends * step)
    outside = highs.astype(np.float32) < 0
    inside = lows.astype(np.float32) > 0
    volume = np.full(shape, -1, dtype=np.float32)
    exact = np.zeros(shape, dtype=bool)
    for start, end, positive, negative in zip(
        starts, ends, inside, outside, strict=True
    ):
        region = tuple(
            slice(int(a), int(b) + 1) for a, b in zip(start, end, strict=True)
        )
        if positive:
            volume[region] = 1
        elif not negative:
            exact[region] = True
    indices = np.flatnonzero(exact)
    evaluate_nodes(field, lower, step, shape, volume.reshape(-1), indices)
    return (
        volume,
        lower,
        step,
        {
            "field_point_evaluations": len(indices),
            "interval_block_evaluations": len(starts),
            "classification_mask_bytes": exact.nbytes,
            "block_size": block_size,
            "outside_blocks": int(np.sum(outside)),
            "inside_blocks": int(np.sum(inside)),
            "mixed_blocks": int(np.sum(~(inside | outside))),
        },
    )
