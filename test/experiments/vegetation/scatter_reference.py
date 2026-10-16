"""Independent Float64 nearest triangles and a differential rectangle form factor."""

import numpy as np


def rectangle_factor(half_width, half_height, distance):
    if min(half_width, half_height, distance) <= 0:
        raise ValueError("positive rectangle half sizes and plane separation required")
    x, y = half_width / distance, half_height / distance
    a, b = np.sqrt(1 + x * x), np.sqrt(1 + y * y)
    return 2 / np.pi * (x / a * np.arctan(y / a) + y / b * np.arctan(x / b))


class Float64Nearest:
    def __init__(self, vertices, faces):
        self.triangles = vertices[faces]
        self.lower, self.upper = self.triangles.min(1), self.triangles.max(1)
        self.known_array_bytes = sum(
            a.nbytes for a in (self.triangles, self.lower, self.upper)
        )

    def one(self, origin, direction, length):
        low, high = np.full_like(self.lower, -np.inf), np.full_like(self.upper, np.inf)
        for axis in range(3):
            if direction[axis] == 0:
                outside = (origin[axis] < self.lower[:, axis]) | (
                    origin[axis] > self.upper[:, axis]
                )
                low[outside, axis], high[outside, axis] = np.inf, -np.inf
            else:
                a = (self.lower[:, axis] - origin[axis]) / direction[axis]
                b = (self.upper[:, axis] - origin[axis]) / direction[axis]
                low[:, axis], high[:, axis] = np.minimum(a, b), np.maximum(a, b)
        possible = np.flatnonzero(
            np.maximum(low.max(1), 0) <= np.minimum(high.min(1), length)
        )
        triangles = self.triangles[possible]
        if not len(triangles):
            return -1, 0.0, 0
        a, b = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        normal = np.cross(a, b)
        denominator = normal @ direction
        distance = np.divide(
            np.einsum("ij,ij->i", triangles[:, 0] - origin, normal),
            denominator,
            out=np.full(len(triangles), np.inf),
            where=denominator != 0,
        )
        relative = (
            origin
            + np.where(np.isfinite(distance), distance, 0)[:, None] * direction
            - triangles[:, 0]
        )
        aa, ab, bb = (np.einsum("ij,ij->i", u, v) for u, v in ((a, a), (a, b), (b, b)))
        ra, rb = np.einsum("ij,ij->i", relative, a), np.einsum("ij,ij->i", relative, b)
        determinant = aa * bb - ab * ab
        u = (bb * ra - ab * rb) / determinant
        v = (aa * rb - ab * ra) / determinant
        valid = (
            (distance > 0)
            & (distance <= length)
            & (u >= -1e-12)
            & (v >= -1e-12)
            & (u + v <= 1 + 1e-12)
        )
        distance[~valid] = np.inf
        chosen = int(np.argmin(distance))
        return (
            (int(possible[chosen]), float(distance[chosen]), len(possible))
            if np.isfinite(distance[chosen])
            else (-1, 0.0, len(possible))
        )

    def query(self, origins, directions, length):
        lengths = np.broadcast_to(length, (len(origins),))
        answers = [
            self.one(o, d, limit)
            for o, d, limit in zip(origins, directions, lengths, strict=True)
        ]
        return (
            np.array([a[0] for a in answers]),
            np.array([a[1] for a in answers]),
            sum(a[2] for a in answers),
        )
