"""C1 blends between shared affine parent planes, with rational Bernstein bounds."""

from dataclasses import dataclass
from math import factorial

import numpy as np


def triangular_bernstein(degree=4):
    powers = np.array(
        [
            (i, j, degree - i - j)
            for i in range(degree + 1)
            for j in range(degree + 1 - i)
        ]
    )
    nodes = powers / degree
    factors = np.array(
        [
            factorial(degree) / np.prod([factorial(int(i)) for i in row])
            for row in powers
        ]
    )
    matrix = factors * np.prod(nodes[:, None, :] ** powers[None, :, :], axis=2)
    inverse = np.linalg.solve(matrix, np.eye(len(nodes)))
    return powers, nodes, inverse


POWERS, NODES, INVERSE = triangular_bernstein()


def affine_basis(xy, origins):
    return np.concatenate((np.ones((*xy.shape[:-1], 1)), xy - origins), axis=-1)


@dataclass
class PlaneField:
    origins: np.ndarray
    plane_ids: np.ndarray
    cut_centres: np.ndarray
    inward_normals: np.ndarray
    scales: np.ndarray

    def __post_init__(self):
        self.origins = np.asarray(self.origins, float)
        self.plane_ids = np.asarray(self.plane_ids, np.int64)
        self.cut_centres = np.asarray(self.cut_centres, float)
        self.inward_normals = np.asarray(self.inward_normals, float)
        self.scales = np.asarray(self.scales, float)
        count = len(self.plane_ids)
        if (
            self.origins.ndim != 2
            or self.origins.shape[1] != 2
            or not len(self.origins)
        ):
            raise ValueError("nonempty plane origins required")
        if (
            self.plane_ids.shape != (count, 2)
            or np.any(self.plane_ids < 0)
            or np.any(self.plane_ids >= len(self.origins))
        ):
            raise ValueError("two explicit existing parent planes per patch required")
        if (
            self.cut_centres.shape != (count, 2, 2)
            or self.inward_normals.shape != (count, 2, 2)
            or self.scales.shape != (count,)
        ):
            raise ValueError(
                "paired cut centres/normals and one scale per patch required"
            )
        if not all(
            np.isfinite(v).all()
            for v in (self.origins, self.cut_centres, self.inward_normals, self.scales)
        ) or np.any(self.scales <= 0):
            raise ValueError("finite geometry and positive patch scales required")

    def local_basis(self, xy, parts):
        xy, parts = np.asarray(xy, float), np.asarray(parts, np.int64)
        if (
            xy.ndim != 2
            or xy.shape[1] != 2
            or parts.shape != (len(xy),)
            or not np.isfinite(xy).all()
        ):
            raise ValueError("finite XY queries and patch ownership required")
        if np.any(parts < 0) or np.any(parts >= len(self.plane_ids)):
            raise ValueError("query patch index out of range")
        parent = self.plane_ids[parts]
        blend = parent[:, 0] != parent[:, 1]
        denominator, weight = np.ones(len(xy)), np.zeros(len(xy))
        gradient_weight = np.zeros_like(xy)
        if blend.any():
            ids = parts[blend]
            gradients = self.inward_normals[ids] / self.scales[ids, None, None]
            signed = np.einsum(
                "nki,nki->nk", xy[blend, None] - self.cut_centres[ids], gradients
            )
            u, d = signed[:, 0], signed.sum(axis=1)
            if np.any(d <= 1e-8):
                raise ValueError("positive rational blend denominator required")
            t = u / d
            dt = (
                gradients[:, 0] * d[:, None] - u[:, None] * gradients.sum(axis=1)
            ) / d[:, None] ** 2
            denominator[blend] = d
            weight[blend] = t * t * (3 - 2 * t)
            gradient_weight[blend] = 6 * (t * (1 - t))[:, None] * dt
        left, right = (
            affine_basis(xy, self.origins[parent[:, side]]) for side in (0, 1)
        )
        base = np.array([[0.0, 1.0, 0.0], [0.0, 0.0, 1.0]])
        height = np.c_[(1 - weight[:, None]) * left, weight[:, None] * right]
        gradient = np.concatenate(
            (
                (1 - weight[:, None, None]) * base
                - gradient_weight[:, :, None] * left[:, None],
                weight[:, None, None] * base
                + gradient_weight[:, :, None] * right[:, None],
            ),
            axis=2,
        )
        columns = (3 * parent[:, :, None] + np.arange(3)).reshape(-1, 6)
        return columns, height, gradient, denominator

    def evaluate(self, xy, parts, coefficients):
        columns, height, gradient, _ = self.local_basis(xy, parts)
        coefficients = np.asarray(coefficients, float)
        if (
            coefficients.shape != (3 * len(self.origins),)
            or not np.isfinite(coefficients).all()
        ):
            raise ValueError("one finite height and two gradients per plane required")
        local = coefficients[columns]
        return np.einsum("ni,ni->n", height, local), np.einsum(
            "nji,ni->nj", gradient, local
        )

    def triangle_controls(self, triangles, parts):
        triangles, parts = np.asarray(triangles, float), np.asarray(parts, np.int64)
        if triangles.shape != (len(parts), 3, 2):
            raise ValueError("triangle XY and explicit patch ownership required")
        points = np.einsum("ki,nij->nkj", NODES, triangles)
        owners = np.repeat(parts, len(NODES))
        columns, height, gradient, d = self.local_basis(points.reshape(-1, 2), owners)
        columns = columns.reshape(-1, len(NODES), 6)[:, 0]
        height = height.reshape(-1, len(NODES), 6)
        gradient = gradient.reshape(-1, len(NODES), 2, 6)
        d = d.reshape(-1, len(NODES))
        _, corner_height, _, corner_d = self.local_basis(
            triangles.reshape(-1, 2), np.repeat(parts, 3)
        )
        corner_d = corner_d.reshape(-1, 3)
        denominator4 = np.prod(corner_d[:, None] ** POWERS[None], axis=2)
        denominator3 = denominator4 * (POWERS[None] / corner_d[:, None]).sum(axis=2) / 4
        controls = np.einsum(
            "ik,nkjl->nijl", INVERSE, gradient * d[:, :, None, None] ** 4
        )
        controls /= denominator4[:, :, None, None]
        linear = np.einsum("ki,nij->nkj", NODES, corner_height.reshape(-1, 3, 6))
        error = np.einsum(
            "ik,nkj->nij", INVERSE, (height - linear) * d[:, :, None] ** 3
        )
        error /= denominator3[:, :, None]
        if not np.isfinite(controls).all() or not np.isfinite(error).all():
            raise ValueError("rational control coefficients exceed numeric domain")
        return columns, controls, error


def control_bounds(field, triangles, parts, coefficients, frames):
    columns, controls, error = field.triangle_controls(triangles, parts)
    local = np.asarray(coefficients)[columns]
    vectors = np.einsum("nkji,ni->nkj", controls, local)
    normalized = np.einsum("nji,nki->nkj", frames, vectors)
    height = abs(np.einsum("nki,ni->nk", error, local)).max(axis=1)
    guard = (
        256
        * np.finfo(float).eps
        * np.maximum(1, np.einsum("nki,ni->nk", abs(error), abs(local)).max(axis=1))
    )
    return np.linalg.norm(normalized, axis=2).max(axis=1), height + guard
