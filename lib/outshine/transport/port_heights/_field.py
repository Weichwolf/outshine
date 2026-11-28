"""C1 blends between shared affine parent planes, with rational Bernstein bounds."""

from dataclasses import dataclass
from math import factorial

import numpy as np

from ._arrays import owned
from ._budget import PortLimit


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


POWERS, NODES, INVERSE = (
    owned(v, v.dtype, v.shape, "Bernstein table") for v in triangular_bernstein()
)


def affine_basis(xy, origins):
    return np.concatenate((np.ones((*xy.shape[:-1], 1)), xy - origins), axis=-1)


@dataclass(frozen=True, slots=True)
class PlaneField:
    """Owned mathematical C1 patch family; evaluation is not a surface contact query."""

    origins: np.ndarray
    plane_ids: np.ndarray
    cut_centres: np.ndarray
    inward_normals: np.ndarray
    scales: np.ndarray
    max_queries: int = 262144

    def __post_init__(self):
        if len(self.origins) > 16384 or len(self.plane_ids) > 8192:
            raise ValueError("bounded plane and patch counts required")
        origins = np.asarray(self.origins)
        parents = np.asarray(self.plane_ids)
        if origins.ndim != 2 or origins.shape[1] != 2 or not 0 < len(origins) <= 16384:
            raise ValueError("bounded nonempty plane origins required")
        if parents.ndim != 2 or parents.shape[1] != 2 or not 0 < len(parents) <= 8192:
            raise ValueError("bounded paired patch parents required")
        count = len(parents)
        for name, dtype, shape, integer in (
            ("origins", float, origins.shape, False),
            ("plane_ids", np.int64, (count, 2), True),
            ("cut_centres", float, (count, 2, 2), False),
            ("inward_normals", float, (count, 2, 2), False),
            ("scales", float, (count,), False),
        ):
            object.__setattr__(
                self,
                name,
                owned(getattr(self, name), dtype, shape, name, integer=integer),
            )
        if np.any(self.plane_ids < 0) or np.any(self.plane_ids >= len(origins)):
            raise ValueError("two explicit existing parent planes per patch required")
        if np.any(self.scales < 1e-8) or np.any(self.scales > 4e7):
            raise ValueError("metric patch scales in [1e-8,4e7] required")
        if max(abs(self.origins).max(), abs(self.cut_centres).max()) > 4e7:
            raise ValueError("bounded metric plane/cut coordinates required")
        blend = self.plane_ids[:, 0] != self.plane_ids[:, 1]
        length = np.linalg.norm(self.inward_normals[blend], axis=2)
        if np.any(abs(length - 1) > 1e-12):
            raise ValueError(
                "actual unit inward normals for blended whole cuts required"
            )
        if type(self.max_queries) is not int or not 0 < self.max_queries <= 1048576:
            raise ValueError("positive query work cap at most 1048576 required")

    def local_basis(self, xy, parts):
        if len(xy) > self.max_queries:
            raise PortLimit("port_query_budget")
        xy, raw_parts = np.asarray(xy, float), np.asarray(parts)
        if xy.ndim != 2 or xy.shape[1] != 2:
            raise ValueError("two-dimensional metric XY queries required")
        if len(xy) > self.max_queries:
            raise PortLimit("port_query_budget")
        if raw_parts.size and raw_parts.dtype.kind not in "iu":
            raise ValueError("integer query module indices without truncation required")
        parts = np.asarray(raw_parts, np.int64)
        if (
            xy.ndim != 2
            or xy.shape[1] != 2
            or parts.shape != (len(xy),)
            or not np.isfinite(xy).all()
            or abs(xy).max(initial=0) > 4e7
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
        coefficients = self._coefficients(coefficients)
        local = coefficients[columns]
        return np.einsum("ni,ni->n", height, local), np.einsum(
            "nji,ni->nj", gradient, local
        )

    def _coefficients(self, coefficients):
        values = np.asarray(coefficients, float)
        if (
            values.shape != (3 * len(self.origins),)
            or not np.isfinite(values).all()
            or abs(values).max() > 4e7
        ):
            raise ValueError(
                "one bounded metric height and two gradients per plane required"
            )
        return values

    def bounds(self, triangles_xy_m, part_ids, coefficients, grade_frames):
        """Bound every analytic gradient and height chord on explicit triangles.

        Positive rational Bernstein denominators give conservative whole-face
        bounds. This does not certify a field's physical domain or finite contacts.
        """
        coefficients = self._coefficients(coefficients)
        frames = np.asarray(grade_frames, float)
        if (
            frames.shape != (len(part_ids), 2, 2)
            or not np.isfinite(frames).all()
            or abs(frames).max(initial=0) > 1e12
            or np.any(np.linalg.det(frames) == 0)
        ):
            raise ValueError(
                "bounded nonsingular 2x2 grade frames per triangle required"
            )
        return control_bounds(self, triangles_xy_m, part_ids, coefficients, frames)

    def triangle_controls(self, triangles, parts):
        if len(parts) * len(NODES) > self.max_queries:
            raise PortLimit("port_triangle_query_budget")
        triangles, raw_parts = np.asarray(triangles, float), np.asarray(parts)
        if raw_parts.ndim != 1 or (raw_parts.size and raw_parts.dtype.kind not in "iu"):
            raise ValueError(
                "integer triangle module ownership without truncation required"
            )
        if len(raw_parts) * len(NODES) > self.max_queries:
            raise PortLimit("port_triangle_query_budget")
        parts = np.asarray(raw_parts, np.int64)
        if triangles.shape != (len(parts), 3, 2):
            raise ValueError("triangle XY and explicit patch ownership required")
        if not np.isfinite(triangles).all() or abs(triangles).max(initial=0) > 4e7:
            raise ValueError("bounded finite metric triangles required")
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
