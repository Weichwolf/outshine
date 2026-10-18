"""Depth-ordered unlit chart composition with bounded triangle/pixel batches."""

import numpy as np
from leaf_sheet import sample_image
from raster_batch import fragments, rasterize


def composite(pixel, depth, samples, size):
    order = np.lexsort((-np.arange(len(pixel)), -depth, pixel))
    pixel, samples = pixel[order], samples[order]
    start = np.r_[0, np.flatnonzero(np.diff(pixel)) + 1]
    group = np.repeat(start, np.diff(np.r_[start, len(pixel)]))
    alpha = samples[:, 3]
    opaque = alpha >= 1
    logarithm = np.log1p(-np.where(opaque, 0, alpha))
    prefix = np.cumsum(logarithm)
    base = np.r_[0, prefix[:-1]]
    before = prefix - logarithm - base[group]
    count = np.cumsum(opaque)
    blocked = count - opaque - np.r_[0, count[:-1]][group]
    transmission = np.where(blocked == 0, np.exp(before), 0)
    output = np.zeros((size * size, samples.shape[-1]))
    np.add.at(output, pixel, samples * transmission[:, None])
    return output.reshape(size, size, -1)


def chart_fragments(mesh, colour, normal, frame, lower, upper, size):
    vertices = mesh["vertices"] @ frame
    corners = mesh["vertices"].reshape(-1, 4, 3)
    u, v = corners[:, 1] - corners[:, 0], corners[:, 3] - corners[:, 0]
    u /= np.linalg.norm(u, axis=1, keepdims=True)
    v /= np.linalg.norm(v, axis=1, keepdims=True)
    n = mesh["normals"].reshape(-1, 6, 3)[:, 0]
    bases = np.stack((u, v, n), axis=-1)
    pieces = []
    total, limit = 0, 16 * size * size
    for triangle, weights, pixel, depth in fragments(
        vertices, mesh["faces"], lower, upper, size, max(262144, size * size)
    ):
        total += len(pixel)
        if total > limit:
            raise ValueError("complete projection exceeds the declared fragment budget")
        uv = np.einsum("ni,nij->nj", weights, mesh["uv"][mesh["faces"][triangle]])
        rgba = sample_image(colour, uv)
        encoded = sample_image(normal, uv)
        if colour.dtype == np.uint8:
            rgba /= 255
        if normal.dtype == np.uint8:
            encoded /= 255
        chart = triangle // 2
        mapped = 2 * encoded[:, :3] - encoded[:, 3, None]
        mapped /= np.maximum(np.linalg.norm(mapped, axis=1, keepdims=True), 1e-12)
        mapped = np.einsum("nij,nj->ni", bases[chart], mapped)
        samples = np.column_stack((rgba, mapped * rgba[:, 3, None], rgba[:, 3]))
        pieces.append((pixel, chart, depth, samples))
    if not pieces:
        return np.empty(0, int), np.empty(0), np.empty((0, 8))
    pixel, chart, depth, samples = (
        np.concatenate([piece[index] for piece in pieces]) for index in range(4)
    )
    order = np.lexsort((np.arange(len(pixel)), chart, pixel))
    keys = pixel[order] * len(corners) + chart[order]
    keep = np.r_[keys[1:] != keys[:-1], True]
    order = order[keep & (samples[order, 3] > 0)]
    return pixel[order], depth[order], samples[order]


def wood_fragments(wood, frame, lower, upper, size):
    projected = wood["vertices"] @ frame
    attributes = np.concatenate(
        (
            wood["normals"].reshape(-1, 3, 3),
            projected[wood["faces"], 2, None],
        ),
        axis=-1,
    )
    if "colour" in wood:
        attributes = np.concatenate((attributes, wood["colour"]), axis=-1)
    covered, shaded = rasterize(
        projected,
        wood["faces"],
        attributes,
        lower,
        upper,
        size,
        max(262144, size * size),
    )
    wood_pixels = np.flatnonzero(covered)
    wood_normals = shaded[..., :3].reshape(-1, 3)[wood_pixels]
    wood_normals /= np.maximum(
        np.linalg.norm(wood_normals, axis=1, keepdims=True), 1e-12
    )
    wood_samples = np.column_stack(
        (
            shaded[..., 4:7].reshape(-1, 3)[wood_pixels]
            if "colour" in wood
            else np.broadcast_to((0.14, 0.105, 0.037), (len(wood_pixels), 3)),
            np.ones(len(wood_pixels)),
            wood_normals,
            np.zeros(len(wood_pixels)),
        )
    )
    depth = shaded[..., 3].ravel()[wood_pixels]
    return wood_pixels, depth, np.column_stack((wood_samples, depth))


def source_image(mesh, maps, wood, frame, lower, upper, size):
    pixel, depth, samples = chart_fragments(
        mesh, maps[0], maps[1], frame, lower, upper, size
    )
    samples = np.column_stack((samples, depth * samples[:, 3]))
    wood_pixel, wood_depth, wood_samples = wood_fragments(
        wood, frame, lower, upper, size
    )
    return composite(
        np.r_[pixel, wood_pixel],
        np.r_[depth, wood_depth],
        np.concatenate((samples, wood_samples)),
        size,
    )
