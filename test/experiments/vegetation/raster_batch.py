"""Orthographic rasterization with bounded batches of triangle/pixel pairs."""

import numpy as np


def fragments(vertices, faces, lower, upper, size, pair_budget=262144):
    if (
        type(size) is not int
        or size <= 0
        or type(pair_budget) is not int
        or pair_budget < size * size
    ):
        raise ValueError("positive image size and room for one full triangle required")
    lower, upper = np.asarray(lower), np.asarray(upper)
    projected = (vertices[:, :2] - lower) / (upper - lower) * size
    triangles = projected[faces]
    first = np.maximum(np.floor(triangles.min(axis=1)).astype(int), 0)
    last = np.minimum(np.ceil(triangles.max(axis=1)).astype(int), size)
    span = np.maximum(last - first, 0)
    counts = span[:, 0] * span[:, 1]
    ab, ac = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    determinants = ab[:, 0] * ac[:, 1] - ab[:, 1] * ac[:, 0]
    counts[np.abs(determinants) < 1e-12] = 0
    cumulative = np.concatenate(([0], np.cumsum(counts)))
    start = 0
    while start < len(faces):
        stop = min(
            len(faces),
            int(
                np.searchsorted(
                    cumulative, cumulative[start] + pair_budget, side="right"
                )
                - 1
            ),
        )
        stop = max(stop, start + 1)
        batch_counts = counts[start:stop]
        total = int(batch_counts.sum())
        if total:
            triangle = np.repeat(np.arange(start, stop), batch_counts)
            index = np.arange(total) - np.repeat(
                np.cumsum(batch_counts) - batch_counts, batch_counts
            )
            x = first[triangle, 0] + index % span[triangle, 0]
            y = first[triangle, 1] + index // span[triangle, 0]
            px = x + 0.5 - triangles[triangle, 0, 0]
            py = y + 0.5 - triangles[triangle, 0, 1]
            b = (px * ac[triangle, 1] - py * ac[triangle, 0]) / determinants[triangle]
            c = (ab[triangle, 0] * py - ab[triangle, 1] * px) / determinants[triangle]
            weights = np.column_stack((1 - b - c, b, c))
            valid = np.all(weights >= -1e-10, axis=1)
            triangle, weights, x, y = (
                triangle[valid],
                weights[valid],
                x[valid],
                y[valid],
            )
            z = np.einsum("ni,ni->n", weights, vertices[faces[triangle], 2])
            pixel = y * size + x
            yield triangle, weights, pixel, z
        start = stop


def rasterize(vertices, faces, attributes, lower, upper, size, pair_budget=262144):
    depth = np.full(size * size, -np.inf)
    output = np.zeros((size * size, attributes.shape[-1]), dtype=np.float32)
    covered = np.zeros(size * size, dtype=bool)
    for triangle, weights, pixel, z in fragments(
        vertices, faces, lower, upper, size, pair_budget
    ):
        if len(pixel):
            np.maximum.at(depth, pixel, z)
            foremost = z >= depth[pixel]
            winners = np.full(size * size, -1, dtype=np.int64)
            np.maximum.at(winners, pixel[foremost], np.flatnonzero(foremost))
            targets = np.flatnonzero(winners >= 0)
            chosen = winners[targets]
            output[targets] = np.einsum(
                "ni,nij->nj", weights[chosen], attributes[triangle[chosen]]
            )
            covered[targets] = True
    return covered.reshape(size, size), output.reshape(size, size, -1)
