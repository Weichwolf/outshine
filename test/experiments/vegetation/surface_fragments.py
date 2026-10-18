"""Bounded curved alpha-surface hits; topology owns shared edges, not positions."""

from dataclasses import dataclass

import numpy as np
from leaf_sheet import sample_image
from raster_batch import fragments
from textured_raster import composite


@dataclass(frozen=True)
class SurfaceHits:
    pixel: np.ndarray
    depth: np.ndarray
    alpha: np.ndarray
    moment: np.ndarray
    owner: np.ndarray
    component: np.ndarray
    face: np.ndarray


def mesh_hits(mesh, camera, bounds, size, *, owner=0, max_fragments=1048576):
    if type(max_fragments) is not int or not 0 <= max_fragments <= 1048576:
        raise ValueError("explicit bounded retained fragment capacity required")
    pieces = []
    total = 0
    for packet in fragments(mesh["vertices"] @ camera, mesh["faces"], *bounds, size):
        total += len(packet[0])
        if total > max_fragments:
            raise ValueError("retained surface fragment capacity exceeded")
        pieces.append(packet)
    if not total:
        return (
            np.empty(0, np.int64),
            np.empty((0, 3)),
            np.empty(0, np.int64),
            np.empty(0),
            np.empty(0, np.int64),
        )
    triangle, weights, pixel, depth = (
        np.concatenate([p[k] for p in pieces]) for k in range(4)
    )
    vertices = mesh["faces"][triangle]
    owners = np.broadcast_to(np.asarray(owner, np.uint64), (len(mesh["faces"]),))[
        triangle
    ]
    # Match the existing raster's1e-10 barycentric boundary policy. No XYZ welding.
    support = np.sort(np.where(weights > 1e-10, vertices, -1), axis=1)
    canonical = np.sort(vertices, axis=1)
    order = np.lexsort(
        (
            canonical[:, 2],
            canonical[:, 1],
            canonical[:, 0],
            support[:, 2],
            support[:, 1],
            support[:, 0],
            owners,
            pixel,
        )
    )
    same = (
        (pixel[order[1:]] == pixel[order[:-1]])
        & (owners[order[1:]] == owners[order[:-1]])
        & np.all(support[order[1:]] == support[order[:-1]], axis=1)
    )
    keep = order[np.r_[True, ~same]]
    faces = np.sort(mesh["faces"], axis=1)
    sorted_faces = np.lexsort((faces[:, 2], faces[:, 1], faces[:, 0]))
    rank = np.empty(len(faces), np.int64)
    rank[sorted_faces] = np.arange(len(faces))
    return triangle[keep], weights[keep], pixel[keep], depth[keep], rank[triangle[keep]]


def surface_hits(
    mesh,
    camera,
    bounds,
    size,
    *,
    owner=0,
    component=0,
    colour=None,
    normal=None,
    basis=None,
    max_fragments=1048576,
):
    triangle, weights, pixel, depth, rank = mesh_hits(
        mesh, camera, bounds, size, owner=owner, max_fragments=max_fragments
    )
    if colour is None:
        moment = np.einsum(
            "ni,nij->nj", weights, mesh["normals"].reshape(-1, 3, 3)[triangle]
        )
        moment /= np.maximum(np.linalg.norm(moment, axis=1, keepdims=True), 1e-12)
        alpha = np.ones(len(pixel))
    else:
        basis = np.asarray(basis, float)
        if (
            basis.shape != (3, 3)
            or not np.allclose(basis.T @ basis, np.eye(3), rtol=0, atol=1e-12)
            or not np.isclose(np.linalg.det(basis), 1, rtol=0, atol=1e-12)
        ):
            raise ValueError("explicit rigid raw-normal encoding basis required")
        uv = np.einsum("ni,nij->nj", weights, mesh["uv"][mesh["faces"][triangle]])
        rgba = sample_image(colour, uv)
        encoded = sample_image(normal, uv)
        np.testing.assert_allclose(rgba[:, 3], encoded[:, 3], rtol=0, atol=1e-12)
        alpha = np.clip(rgba[:, 3], 0, 1)
        moment = (2 * encoded[:, :3] - encoded[:, 3, None]) @ basis.T
    owners = np.broadcast_to(np.asarray(owner, np.uint64), (len(mesh["faces"]),))[
        triangle
    ]
    return SurfaceHits(
        pixel,
        depth,
        alpha,
        moment,
        owners,
        np.full(len(pixel), component, np.int64),
        rank,
    )


def resolve(hits, lights, size, *, max_fragments=1048576):
    """Current unit front/back cosine response, before any spatial pixel averaging."""
    lights = np.asarray(lights, float)
    if (
        lights.ndim != 2
        or lights.shape[1] != 3
        or not 1 <= len(lights) <= 24
        or not np.allclose(np.linalg.norm(lights, axis=1), 1, rtol=0, atol=1e-12)
    ):
        raise ValueError("bounded unit light directions required")
    total = sum(len(h.pixel) for h in hits)
    if total > max_fragments:
        raise ValueError("retained group fragment capacity exceeded")
    if not total:
        return np.zeros((size, size, 1 + 2 * len(lights)))
    arrays = [
        np.concatenate([getattr(h, k) for h in hits])
        for k in ("pixel", "depth", "alpha", "moment", "owner", "component", "face")
    ]
    pixel, depth, alpha, moment, owner, component, face = arrays
    nonzero = alpha > 0
    if not np.any(nonzero):
        return np.zeros((size, size, 1 + 2 * len(lights)))
    # The existing compositor breaks depth ties with last input: smallest stable ID wins.
    order = np.flatnonzero(nonzero)
    order = order[np.lexsort((-face[order], -component[order], ~owner[order]))]
    cosine = moment[order] @ lights.T
    samples = np.column_stack(
        (
            np.zeros((len(order), 3)),
            alpha[order],
            np.maximum(cosine, 0),
            np.maximum(-cosine, 0),
        )
    )
    result = composite(pixel[order], depth[order], samples, size)
    return result[..., 3:]
