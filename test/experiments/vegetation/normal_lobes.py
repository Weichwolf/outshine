"""Positive clamped first moments, with a finite-measure Jensen-gap bound."""

import numpy as np
from needle_response import source_samples


def bins(normals, count, frame):
    if count == 4:
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


def evaluate(vectors, directions):
    vectors, directions = np.asarray(vectors, float), np.asarray(directions, float)
    if (
        vectors.ndim != 3
        or vectors.shape[-1] != 3
        or vectors.shape[1] > 24
        or vectors.shape[0] > 4096
        or not np.isfinite(vectors).all()
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


def error_bound(moments):
    moments = np.asarray(moments, float)
    if moments.ndim != 3 or moments.shape[-1] != 4 or not np.isfinite(moments).all():
        raise ValueError("finite mass/first-moment fields required")
    mass = moments[..., 0]
    vectors = moments[..., 1:]
    if np.any(mass < 0) or np.any(
        np.linalg.norm(vectors, axis=-1) > mass * (1 + 2e-12)
    ):
        raise ValueError(
            "first moments must belong to a nonnegative unit-normal measure"
        )
    variance = np.maximum(mass * mass - np.sum(vectors * vectors, axis=-1), 0)
    return 0.5 * np.sqrt(variance).sum(axis=-1)


def quantization_bound(vectors, packed):
    return np.linalg.norm(np.asarray(packed, float) - vectors, axis=-1).sum(axis=-1)


def mass_preserving_vectors(moments):
    error_bound(moments)
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


def mass_preserving_bound(moments):
    error_bound(moments)
    mass = moments[..., 0]
    length = np.linalg.norm(moments[..., 1:], axis=-1)
    gap = 0.5 * np.sqrt(np.maximum(mass * mass - length * length, 0))
    return np.maximum(gap, np.maximum(mass - length, 0)).sum(axis=-1)


class NormalMeasure:
    def __init__(self, size, directions, frame=None):
        if type(size) is not int or size not in (1, 4, 16, 64):
            raise ValueError("bounded native normal field required")
        self.size = size
        if not 1 <= len(directions) <= 128:
            raise ValueError("bounded angular query set required")
        self.directions = np.array(directions, dtype=float, copy=True)
        evaluate(np.zeros((1, 1, 3)), self.directions)
        self.frame = (
            np.eye(3) if frame is None else np.array(frame, dtype=float, copy=True)
        )
        if (
            self.frame.shape != (3, 3)
            or not np.isfinite(self.frame).all()
            or not np.allclose(self.frame.T @ self.frame, np.eye(3), atol=1e-12, rtol=0)
        ):
            raise ValueError("orthonormal normal-partition frame required")
        self.directions.setflags(write=False)
        self.frame.setflags(write=False)
        self.moments = {
            count: np.zeros((size * size, count, 4)) for count in (4, 6, 24)
        }
        self.exact = np.zeros((len(self.directions), size * size))
        self.sample_count = 0

    def add(self, slots, mass, normals):
        if (
            slots.ndim != 1
            or slots.dtype.kind not in "iu"
            or len(slots) > 8192
            or slots.min(initial=0) < 0
            or slots.max(initial=-1) >= self.size * self.size
            or self.sample_count + len(slots) > 2_000_000
        ):
            raise ValueError("bounded same-field normal sample packet required")
        normals, mass = source_samples(normals, mass)
        if len(normals) != len(slots) or np.any(mass > 1 + 1e-12):
            raise ValueError("same projected-area source contacts required")
        if not len(slots):
            return
        self.sample_count += len(slots)
        for count, field in self.moments.items():
            index = slots * count + bins(normals, count, self.frame)
            for channel, weights in enumerate(
                (mass, *(mass * normals[:, axis] for axis in range(3)))
            ):
                field[..., channel] += np.bincount(
                    index, weights=weights, minlength=self.size * self.size * count
                ).reshape(self.size * self.size, count)
        for first in range(0, len(self.directions), 8):
            values = mass[:, None] * np.maximum(
                normals @ self.directions[first : first + 8].T, 0
            )
            for index, column in enumerate(values.T):
                self.exact[first + index] += np.bincount(
                    slots, weights=column, minlength=self.size * self.size
                )
