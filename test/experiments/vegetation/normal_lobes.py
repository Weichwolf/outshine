"""Diagnostic visible-source collector; reusable mathematics lives in the library."""

import numpy as np
from needle_response import source_samples
from outshine_ref.vegetation.normal_distribution import (
    evaluate_diffuse as evaluate,
)
from outshine_ref.vegetation.normal_distribution import (
    jensen_error_bound as error_bound,
)
from outshine_ref.vegetation.normal_distribution import (
    mass_preserving_error_bound as mass_preserving_bound,
)
from outshine_ref.vegetation.normal_distribution import (
    mass_preserving_vectors,
)
from outshine_ref.vegetation.normal_distribution import (
    normal_partition_ids as bins,
)
from outshine_ref.vegetation.normal_distribution import (
    quantization_error_bound as quantization_bound,
)

__all__ = [
    "NormalMeasure",
    "bins",
    "error_bound",
    "evaluate",
    "mass_preserving_bound",
    "mass_preserving_vectors",
    "quantization_bound",
]


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
