"""Own outward ring attachment for procedural fin meshes, with exact warp normals."""

from dataclasses import dataclass
from functools import cached_property

import numpy as np

from .spatial_axis import SpatialAxis
from .spine_frames import unit


def real_points(values):
    try:
        if len(values) > 786432:
            raise ValueError("complete fin data exceeds its row budget")
    except TypeError as error:
        raise ValueError("bounded point rows required") from error
    values = np.asarray(values)
    if values.dtype.kind not in "fiu" or values.ndim != 2 or values.shape[1] != 3:
        raise ValueError("real three-dimensional point rows required")
    values = np.asarray(values, dtype=np.float64)
    if not np.isfinite(values).all():
        raise ValueError("finite point rows required")
    return values


@dataclass(frozen=True)
class FinAttachment:
    parent: SpatialAxis
    fraction: float = 0.5
    azimuth: float = 0.0
    start_angle: float = 0.0

    def __post_init__(self):
        if not isinstance(self.parent, SpatialAxis):
            raise TypeError("prepared spatial parent axis required")
        if (
            any(
                isinstance(v, (bool, np.bool_))
                for v in (self.fraction, self.azimuth, self.start_angle)
            )
            or not np.isfinite([self.fraction, self.azimuth, self.start_angle]).all()
            or not 0 <= self.fraction <= 1
            or abs(self.azimuth) > 2 * np.pi
            or abs(self.start_angle) > np.pi
        ):
            raise ValueError(
                "finite attachment fraction and bounded radian angles required"
            )

    def recipe(self):
        return {
            "parent": self.parent.recipe(),
            "fraction": float(self.fraction),
            "azimuth": float(self.azimuth),
            "start_angle": float(self.start_angle),
        }

    @classmethod
    def from_recipe(cls, record):
        return cls(
            SpatialAxis.from_recipe(record["parent"]),
            record["fraction"],
            record["azimuth"],
            record["start_angle"],
        )

    @cached_property
    def _slice(self):
        sample = self.parent.field.sample(self.fraction)
        vectors = np.stack(
            [sample[key] for key in ("centre", "lateral", "tangent", "normal")]
        )
        vectors = np.frombuffer(vectors.tobytes(), dtype=np.float64).reshape(4, 3)
        radius = float(self.parent.radius.evaluate(self.fraction))
        a, b = self.parent.ellipse
        reference_radius = radius * np.sqrt(a) * np.sqrt(b)
        if not np.isfinite(reference_radius) or reference_radius <= 0:
            raise ValueError("finite positive representable attachment radius required")
        return vectors, radius, float(reference_radius)

    def _rotate(self, values):
        cosine, sine = np.cos(self.start_angle), np.sin(self.start_angle)
        return np.column_stack(
            (
                values[:, 0],
                cosine * values[:, 1] - sine * values[:, 2],
                sine * values[:, 1] + cosine * values[:, 2],
            )
        )

    def _field(self, points):
        if len(points) > 262144:
            raise ValueError("complete fin point request exceeds its budget")
        points = self._rotate(real_points(points))
        if not len(points):
            return tuple(np.empty((0, 3)) for _ in range(4))
        vectors, radius, reference_radius = self._slice
        centre, lateral, tangent, normal = vectors
        if np.any(np.abs(points[:, 0]) >= np.pi * reference_radius) or np.any(
            points[:, 1] < 0
        ):
            raise ValueError(
                "open single-turn X interval and outward fin coordinates required"
            )
        angle = self.azimuth + points[:, 0] / reference_radius
        a, b = self.parent.ellipse
        cosine, sine = np.cos(angle), np.sin(angle)
        radial = cosine[:, None] * a * lateral + sine[:, None] * b * normal
        derivative = -sine[:, None] * a * lateral + cosine[:, None] * b * normal
        n = cosine[:, None] / a * lateral + sine[:, None] / b * normal
        n_d = -sine[:, None] / a * lateral + cosine[:, None] / b * normal
        norm = np.hypot(cosine / a, sine / b)
        outward = n / norm[:, None]
        outward_d = (n_d - np.sum(outward * n_d, axis=1)[:, None] * outward) / norm[
            :, None
        ]
        positions = (
            centre
            + radius * radial
            + points[:, 1, None] * outward
            + points[:, 2, None] * tangent
        )
        dx = (radius * derivative + points[:, 1, None] * outward_d) / reference_radius
        dz = np.broadcast_to(tangent, dx.shape)
        determinant = np.sum(dx * np.cross(outward, dz), axis=1)
        if (
            not np.isfinite(positions).all()
            or not np.isfinite(dx).all()
            or not np.isfinite(outward).all()
            or not np.all(determinant > 0)
        ):
            raise ValueError("finite regular outward fin map required")
        return positions, dx, outward, dz

    def positions(self, points):
        return self._field(points)[0]

    def jacobian(self, points):
        _, a, b, c = self._field(points)
        cosine, sine = np.cos(self.start_angle), np.sin(self.start_angle)
        return np.stack((a, cosine * b + sine * c, -sine * b + cosine * c), axis=2)

    def normals(self, points, normals, method="cofactor"):
        normals = unit(real_points(normals))
        points = real_points(points)
        if points.shape != normals.shape:
            raise ValueError("one source normal per requested point required")
        _, a, b, c = self._field(points)
        return self._transform_normals(a, b, c, normals, method)

    def _transform_normals(self, a, b, c, normals, method):
        normals = self._rotate(unit(normals))
        if method == "solve":
            matrix = np.stack((a, b, c), axis=1)
            return unit(np.linalg.solve(matrix, normals[..., None])[..., 0])
        if method != "cofactor":
            raise ValueError("declared fin normal transform method required")
        transformed = (
            normals[:, 0, None] * np.cross(b, c)
            + normals[:, 1, None] * np.cross(c, a)
            + normals[:, 2, None] * np.cross(a, b)
        )
        return unit(transformed)

    def deform(self, mesh, max_faces=262144, method="cofactor"):
        if type(max_faces) is not int or not 1 <= max_faces <= 262144:
            raise ValueError("bounded complete face budget required")
        if (
            not 1 <= len(mesh["vertices"]) <= 65536
            or not 1 <= len(mesh["faces"]) <= max_faces
        ):
            raise ValueError("complete fin mesh exceeds its detail budget")
        points = real_points(mesh["vertices"])
        faces = np.asarray(mesh["faces"])
        normals = real_points(mesh["normals"])
        if (
            faces.ndim != 2
            or faces.shape[1] != 3
            or faces.dtype.kind not in "iu"
            or faces.min() < 0
            or faces.max() >= len(points)
            or normals.shape != (faces.size, 3)
        ):
            raise ValueError(
                "valid triangular indices and complete corner normals required"
            )
        _, _, reference_radius = self._slice
        if np.any(np.ptp(points[faces, 0], axis=1) / reference_radius > np.pi / 2):
            raise ValueError(
                "fin detail does not resolve circumferential anchor variation"
            )
        positions, a, b, c = self._field(points)
        indices = faces.ravel()
        corners = self._transform_normals(
            a[indices], b[indices], c[indices], normals, method
        ).reshape(-1, 3, 3)
        p = positions[faces]
        cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        if not np.all(np.sum(cross[:, None] * corners, axis=2) > 0):
            raise ValueError("fin detail does not resolve the oriented surface")
        result = {key: np.array(value, copy=True) for key, value in mesh.items()}
        vectors, _radius, reference_radius = self._slice
        offset = positions - vectors[0]
        result.update(
            vertices=positions,
            normals=corners.reshape(-1, 3),
            faces=faces.astype(np.int32),
            bark_coordinates=np.column_stack(
                (
                    offset @ vectors[1],
                    offset @ vectors[3],
                    self.fraction * self.parent.length + offset @ vectors[2],
                )
            ),
            bark_radius=np.full(len(points), reference_radius),
        )
        return result
