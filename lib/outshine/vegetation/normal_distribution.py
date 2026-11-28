"""Positive clamped first moments, with a finite-measure Jensen-gap bound."""

import numpy as np


def normal_partition_ids(normals, count, frame):
    normals = np.asarray(normals, float)
    if (
        type(count) is not int
        or count not in (4, 6, 24)
        or normals.ndim != 2
        or normals.shape[1] != 3
        or len(normals) > 8192
        or not np.isfinite(normals).all()
        or np.any(abs(np.linalg.norm(normals, axis=1) - 1) > 1e-10)
    ):
        raise ValueError("bounded unit-normal packet and4/6/24 partitions required")
    if count == 4:
        frame = np.asarray(frame, float)
        if (
            frame.shape != (3, 3)
            or not np.isfinite(frame).all()
            or not np.allclose(frame.T @ frame, np.eye(3), atol=1e-12, rtol=0)
        ):
            raise ValueError("orthonormal quadrant frame required")
        local = normals @ frame
        return (local[:, 0] < 0).astype(int) + 2 * (local[:, 1] < 0)
    subdivision = 1 if count == 6 else 2
    axis = np.argmax(abs(normals), axis=1)
    rows = np.arange(len(normals))
    dominant = normals[rows, axis]
    u = normals[rows, (axis + 1) % 3] / abs(dominant)
    v = normals[rows, (axis + 2) % 3] / abs(dominant)
    x = np.clip(np.floor((u + 1) * subdivision / 2).astype(int), 0, subdivision - 1)
    y = np.clip(np.floor((v + 1) * subdivision / 2).astype(int), 0, subdivision - 1)
    face = 2 * axis + (dominant < 0)
    return face * subdivision**2 + y * subdivision + x


def evaluate_diffuse(vectors, directions):
    vectors, directions = np.asarray(vectors, float), np.asarray(directions, float)
    if (
        vectors.ndim != 3
        or vectors.shape[-1] != 3
        or not 1 <= vectors.shape[1] <= 24
        or not 1 <= vectors.shape[0] <= 4096
        or not np.isfinite(vectors).all()
        or np.max(abs(vectors), initial=0) > 2_000_000
        or directions.ndim != 2
        or directions.shape[1] != 3
        or not 1 <= len(directions) <= 128
        or not np.isfinite(directions).all()
        or np.any(abs(np.linalg.norm(directions, axis=1) - 1) > 1e-10)
    ):
        raise ValueError("bounded finite first moments and unit light queries required")
    result = np.empty((len(directions), len(vectors)))
    for first in range(0, len(directions), 8):
        dots = np.einsum("pbj,kj->kpb", vectors, directions[first : first + 8])
        result[first : first + 8] = np.maximum(dots, 0).sum(axis=2)
    return result


def jensen_error_bound(moments):
    moments = np.asarray(moments, float)
    if (
        moments.ndim != 3
        or moments.shape[-1] != 4
        or not 1 <= moments.shape[0] <= 4096
        or not 1 <= moments.shape[1] <= 24
        or not np.isfinite(moments).all()
    ):
        raise ValueError("finite mass/first-moment fields required")
    mass = moments[..., 0]
    vectors = moments[..., 1:]
    if (
        np.any(mass < 0)
        or np.any(mass > 2_000_000)
        or np.any(np.linalg.norm(vectors, axis=-1) > mass * (1 + 2e-12))
    ):
        raise ValueError(
            "first moments must belong to a nonnegative unit-normal measure"
        )
    variance = np.maximum(mass * mass - np.sum(vectors * vectors, axis=-1), 0)
    return 0.5 * np.sqrt(variance).sum(axis=-1)


def quantization_error_bound(vectors, packed):
    vectors, packed = np.asarray(vectors, float), np.asarray(packed, float)
    if (
        vectors.ndim != 3
        or vectors.shape[-1] != 3
        or vectors.shape != packed.shape
        or not 1 <= vectors.shape[0] <= 4096
        or not 1 <= vectors.shape[1] <= 24
        or not np.isfinite(vectors).all()
        or not np.isfinite(packed).all()
        or np.max(abs(vectors), initial=0) > 2_000_000
        or np.max(abs(packed), initial=0) > 2_000_000
    ):
        raise ValueError("matching finite bounded vector encodings required")
    return np.linalg.norm(np.asarray(packed, float) - vectors, axis=-1).sum(axis=-1)


def mass_preserving_vectors(moments):
    moments = np.asarray(moments, float)
    jensen_error_bound(moments)
    mass = moments[..., 0]
    vectors = moments[..., 1:]
    length = np.linalg.norm(vectors, axis=-1)
    if np.any((mass > 0) & (length == 0)):
        raise ValueError("zero-resultant positive mass needs a richer distribution")
    return np.divide(
        vectors * mass[..., None],
        length[..., None],
        out=np.zeros_like(vectors),
        where=length[..., None] > 0,
    )


def mass_preserving_error_bound(moments):
    moments = np.asarray(moments, float)
    jensen_error_bound(moments)
    mass = moments[..., 0]
    length = np.linalg.norm(moments[..., 1:], axis=-1)
    gap = 0.5 * np.sqrt(np.maximum(mass * mass - length * length, 0))
    return np.maximum(gap, np.maximum(mass - length, 0)).sum(axis=-1)
