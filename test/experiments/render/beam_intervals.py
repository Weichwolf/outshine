"""Outward Float64 beam bounds over an observer box and angular rectangle."""

import itertools

import numpy as np


def exact(value):
    value = np.asarray(value, float)
    return value, value


def add(a, b):
    return np.nextafter(a[0] + b[0], -np.inf), np.nextafter(a[1] + b[1], np.inf)


def sub(a, b):
    return np.nextafter(a[0] - b[1], -np.inf), np.nextafter(a[1] - b[0], np.inf)


def mul(a, b):
    values = [a[i] * b[j] for i in (0, 1) for j in (0, 1)]
    return np.nextafter(np.minimum.reduce(values), -np.inf), np.nextafter(
        np.maximum.reduce(values), np.inf
    )


def divide(a, b):
    if np.any((b[0] <= 0) & (b[1] >= 0)):
        raise ValueError("beam interval denominator crosses zero")
    values = [a[i] / b[j] for i in (0, 1) for j in (0, 1)]
    return np.nextafter(np.minimum.reduce(values), -np.inf), np.nextafter(
        np.maximum.reduce(values), np.inf
    )


def component(a, axis):
    return a[0][..., axis], a[1][..., axis]


def cross(a, b):
    result = [
        sub(
            mul(component(a, (i + 1) % 3), component(b, (i + 2) % 3)),
            mul(component(a, (i + 2) % 3), component(b, (i + 1) % 3)),
        )
        for i in range(3)
    ]
    return tuple(np.stack([r[i] for r in result], axis=-1) for i in (0, 1))


def dot(a, b):
    result = exact(np.zeros(np.broadcast_shapes(a[0].shape[:-1], b[0].shape[:-1])))
    for i in range(3):
        result = add(result, mul(component(a, i), component(b, i)))
    return result


def projector(positions, observer_box):
    """A signed axis permutation preserves the represented coordinates exactly."""
    delta = np.mean(positions, axis=0) - np.mean(observer_box, axis=0)
    axis = int(np.argmax(abs(delta)))
    forward = np.eye(3)[axis] * np.sign(delta[axis])
    right = np.eye(3)[(axis + 1) % 3]
    return np.stack((right, np.cross(forward, right), forward))


class Beams:
    def __init__(self, positions, indices, observer_box):
        positions, indices, observer_box = (
            np.asarray(positions),
            np.asarray(indices),
            np.asarray(observer_box, float),
        )
        if (
            positions.ndim != 2
            or positions.shape[1:] != (3,)
            or not len(positions)
            or not np.isfinite(positions).all()
            or abs(positions).max() > 1e7
            or indices.ndim != 2
            or indices.shape[1:] != (3,)
            or not 0 < len(indices) <= 131072
            or not np.issubdtype(indices.dtype, np.integer)
            or indices.min() < 0
            or indices.max() >= len(positions)
            or observer_box.shape != (2, 3)
            or not np.isfinite(observer_box).all()
            or np.any(observer_box[0] > observer_box[1])
            or abs(observer_box).max() > 1e7
        ):
            raise ValueError(
                "bounded finite original geometry and observer box required"
            )
        self.basis = projector(positions, observer_box)
        vertices = np.asarray(positions, float) @ self.basis.T
        eyes = np.asarray(observer_box, float) @ self.basis.T
        self.observer = np.stack((eyes.min(0), eyes.max(0)))
        triangle = vertices[indices]
        eye = self.observer[0], self.observer[1]
        a, b, c = (exact(triangle[:, i]) for i in range(3))
        self.normal = cross(sub(b, a), sub(c, a))
        self.numerator = dot(self.normal, sub(a, eye))
        corners = np.array(list(itertools.product(*zip(*self.observer, strict=True))))
        fixed_eyes = exact(corners[:, None])
        edges = [
            cross(sub(v, u), sub(fixed_eyes, u)) for u, v in ((a, b), (b, c), (c, a))
        ]
        self.edges = tuple(
            np.stack([edge[i] for edge in edges], axis=-2) for i in (0, 1)
        )
        self.depth = (
            np.nextafter(triangle[:, :, 2].min(1) - eye[1][2], -np.inf),
            np.nextafter(triangle[:, :, 2].max(1) - eye[0][2], np.inf),
        )
        if self.depth[0].min() <= 0.2:
            raise ValueError("group needs an original or different directional domain")
        relative = sub(exact(vertices), eye)
        denominator = component(relative, 2)
        lower, upper = [], []
        for i in (0, 1):
            lo, hi = divide(component(relative, i), denominator)
            lower.append(lo.min())
            upper.append(hi.max())
        self.bounds = np.nextafter(
            np.nextafter(np.array([lower, upper]), [[-np.inf], [np.inf]]),
            [[-np.inf], [np.inf]],
        )
        self.prepared_array_bytes = sum(
            a.nbytes
            for pair in (self.normal, self.numerator, self.edges, self.depth)
            for a in pair
        )

    def classify(self, ids, box):
        # Direction division at runtime may round by an ulp at a tree boundary.
        low = np.nextafter(np.nextafter(box[0], -np.inf), -np.inf)
        high = np.nextafter(np.nextafter(box[1], np.inf), np.inf)
        direction = np.r_[low, 1.0], np.r_[high, 1.0]
        n = tuple(a[ids] for a in self.normal)
        denominator = dot(n, direction)
        edges = dot(
            tuple(a[:, ids] for a in self.edges),
            (direction[0], direction[1]),
        )
        edges = edges[0].min(0), edges[1].max(0)
        # Include the reference triangle routine's barycentric boundary tolerance.
        tolerance = np.nextafter(
            1e-11 * np.maximum(abs(denominator[0]), abs(denominator[1])), np.inf
        )
        possible = (edges[0] <= tolerance[:, None]).all(1) & (
            self.numerator[0][ids] < 0
        )
        full = (
            (edges[1] < -tolerance[:, None]).all(1)
            & (self.numerator[1][ids] < 0)
            & (denominator[1] < 0)
        )
        low_t, high_t = (a[ids].copy() for a in self.depth)
        safe = (denominator[0] > 0) | (denominator[1] < 0)
        if safe.any():
            lo, hi = divide(
                tuple(a[ids[safe]] for a in self.numerator),
                tuple(a[safe] for a in denominator),
            )
            low_t[safe] = np.maximum(low_t[safe], lo)
            high_t[safe] = np.minimum(high_t[safe], hi)
        possible &= low_t <= high_t
        full &= possible
        return possible, full, low_t, high_t
