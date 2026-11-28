"""Metric circular frond cross-sections, with stable positions and Jacobian normals."""

from dataclasses import dataclass

import numpy as np

from .frond_surface import MAX_TRIANGLES, MAX_VERTICES
from .spine_frames import unit
from .vector3 import cross3


def point_rows(values, maximum):
    try:
        if len(values) > maximum:
            raise ValueError("complete curl request exceeds its row budget")
    except TypeError as error:
        raise ValueError("bounded three-dimensional rows required") from error
    values = np.asarray(values)
    if values.ndim != 2 or values.shape[1] != 3 or values.dtype.kind not in "fiu":
        raise ValueError("real three-dimensional rows required")
    values = values.astype(np.float64, copy=False)
    if not np.isfinite(values).all():
        raise ValueError("finite representable rows required")
    return values


@dataclass(frozen=True)
class FrondCurl:
    curvature_per_m: float = 0.0

    def __post_init__(self):
        if isinstance(self.curvature_per_m, (bool, np.bool_)) or not np.isfinite(
            self.curvature_per_m
        ):
            raise ValueError("finite inverse-metre cross-section curvature required")

    def recipe(self):
        return {
            "version": "frond-cross-curl-1",
            "curvature_per_m": float(self.curvature_per_m),
        }

    @classmethod
    def from_recipe(cls, recipe):
        if (
            set(recipe) != {"version", "curvature_per_m"}
            or recipe["version"] != "frond-cross-curl-1"
        ):
            raise ValueError("complete supported frond curl recipe required")
        return cls(recipe["curvature_per_m"])

    def _field(self, points):
        points = point_rows(points, MAX_VERTICES)
        if self.curvature_per_m == 0:
            return points.copy(), np.broadcast_to(np.eye(3), (len(points), 3, 3)).copy()
        k = self.curvature_per_m
        with np.errstate(over="ignore", invalid="ignore"):
            theta = k * points[:, 0]
            factor = 1 - k * points[:, 2]
        if (
            not np.isfinite(theta).all()
            or not np.isfinite(factor).all()
            or np.any(abs(theta) >= np.pi)
            or np.any(factor <= 0)
        ):
            raise ValueError("regular open single-turn curl domain required")
        sine, cosine = np.sin(theta), np.cos(theta)
        positions = points.copy()
        positions[:, 0] = points[:, 0] * np.sinc(theta / np.pi) - points[:, 2] * sine
        positions[:, 2] = (
            points[:, 0] * (theta / 2 * np.sinc(theta / (2 * np.pi)) ** 2)
            + points[:, 2] * cosine
        )
        jacobian = np.zeros((len(points), 3, 3))
        jacobian[:, 0, 0] = factor * cosine
        jacobian[:, 2, 0] = factor * sine
        jacobian[:, 1, 1] = 1
        jacobian[:, 0, 2] = -sine
        jacobian[:, 2, 2] = cosine
        if not np.isfinite(positions).all():
            raise ValueError("finite representable curled positions required")
        return positions, jacobian

    def positions(self, points):
        return self._field(points)[0]

    def jacobian(self, points):
        return self._field(points)[1]

    def deform(self, mesh, method="cofactor"):
        if method not in ("cofactor", "solve"):
            raise ValueError("declared curl normal method required")
        if len(mesh["vertices"]) > MAX_VERTICES or len(mesh["faces"]) > MAX_TRIANGLES:
            raise ValueError("complete curl mesh exceeds its detail budget")
        vertices = point_rows(mesh["vertices"], MAX_VERTICES)
        normals = unit(point_rows(mesh["normals"], 3 * MAX_TRIANGLES))
        faces = np.asarray(mesh["faces"])
        if (
            faces.ndim != 2
            or faces.shape[1] != 3
            or faces.dtype.kind not in "iu"
            or normals.shape != (faces.size, 3)
            or np.any(faces < 0)
            or np.any(faces >= len(vertices))
        ):
            raise ValueError(
                "valid triangular indices and complete corner normals required"
            )
        positions, jacobian = self._field(vertices)
        if np.any(
            np.ptp((self.curvature_per_m * vertices[:, 0])[faces], axis=1) > np.pi / 2
        ):
            raise ValueError("curl mesh does not resolve circumferential variation")
        indices = faces.ravel()
        if method == "solve":
            mapped = np.linalg.solve(
                jacobian[indices].transpose(0, 2, 1), normals[..., None]
            )[..., 0]
        else:
            columns = jacobian.transpose(0, 2, 1)
            cofactors = np.stack(
                (
                    cross3(columns[:, 1], columns[:, 2]),
                    cross3(columns[:, 2], columns[:, 0]),
                    cross3(columns[:, 0], columns[:, 1]),
                ),
                axis=-1,
            )
            mapped = np.einsum("nij,nj->ni", cofactors[indices], normals)
        mapped = unit(mapped)
        p = positions[faces]
        cross = cross3(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        if not np.all(np.sum(cross[:, None] * mapped.reshape(-1, 3, 3), axis=2) > 0):
            raise ValueError("curl mesh does not resolve the oriented surface")
        result = {key: np.array(value, copy=True) for key, value in mesh.items()}
        return result | {
            "vertices": positions,
            "normals": mapped,
            "faces": faces.astype(np.int32),
        }
