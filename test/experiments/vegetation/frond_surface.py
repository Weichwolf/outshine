"""Stable guide deformation of own frond meshes with differential normals."""

import hashlib
import json
from dataclasses import dataclass, field
from functools import cached_property

import numpy as np
from curve_frame import arc_frame
from profile_curve import Profile

MAX_VERTICES = 1_000_000
MAX_TRIANGLES = 2_000_000


def constant_profile():
    return Profile(np.ones(4), "maximum")


@dataclass(frozen=True)
class FrondGuide:
    length: float = 1.3
    bend: float = -0.65
    fold_per_m: float = 0.5
    width: Profile = field(default_factory=constant_profile)
    height: Profile = field(default_factory=constant_profile)
    roll: Profile = field(default_factory=lambda: Profile(np.zeros(4), "minimum"))

    def __post_init__(self):
        if not all(np.isfinite(v) for v in (self.length, self.bend, self.fold_per_m)):
            raise ValueError("finite physical frond guide required")
        if self.length <= 0 or abs(self.bend) > np.pi:
            raise ValueError("positive guide length and bounded bend required")
        if not all(
            isinstance(p, Profile) for p in (self.width, self.height, self.roll)
        ):
            raise ValueError("prepared immutable guide profiles required")
        if min(self.width.table.min(), self.height.table.min()) <= 0:
            raise ValueError("positive transverse scales preserve guide regularity")

    def recipe(self):
        return {
            "version": "frond-guide-1",
            "length_m": self.length,
            "bend_rad": self.bend,
            "fold_per_m": self.fold_per_m,
            "profiles": {
                name: {"kind": p.kind, "table": p.table.tolist()}
                for name, p in (
                    ("width", self.width),
                    ("height", self.height),
                    ("roll", self.roll),
                )
            },
        }

    @classmethod
    def from_recipe(cls, recipe):
        if recipe["version"] != "frond-guide-1":
            raise ValueError("supported complete guide recipe required")
        profiles = {
            name: Profile(np.array(value["table"]), value["kind"])
            for name, value in recipe["profiles"].items()
        }
        return cls(
            recipe["length_m"], recipe["bend_rad"], recipe["fold_per_m"], **profiles
        )

    @cached_property
    def identity(self):
        return hashlib.sha256(
            json.dumps(self.recipe(), sort_keys=True).encode()
        ).hexdigest()

    def validate(self, vertices):
        if not np.isfinite(vertices).all() or np.any(
            (vertices[:, 1] < 0) | (vertices[:, 1] > self.length)
        ):
            raise ValueError(
                "finite source vertices inside the complete guide required"
            )
        if not len(vertices):
            return
        extent = np.max(abs(vertices), axis=0)
        transverse = extent[0] * self.width.table.max()
        depth = (
            extent[2] * self.height.table.max() + abs(self.fold_per_m) * transverse**2
        )
        if abs(self.bend) * np.hypot(transverse, depth) >= self.length:
            raise ValueError(
                "source bounds must preserve the guide Jacobian orientation"
            )

    def evaluate(self, points):
        t = points[:, 1] / self.length
        centre, tangent, lateral, normal = arc_frame(self.length, self.bend, 0, t)
        angle, rate = self.roll.evaluate(t), self.roll.derivative(t)
        cosine, sine = np.cos(angle)[:, None], np.sin(angle)[:, None]
        a, b = cosine * lateral + sine * normal, -sine * lateral + cosine * normal
        da = rate[:, None] * b - self.bend * sine * tangent
        db = -rate[:, None] * a - self.bend * cosine * tangent
        width, height = self.width.evaluate(t), self.height.evaluate(t)
        q = points[:, 0] * width
        d = points[:, 2] * height + self.fold_per_m * q**2
        dq = points[:, 0] * self.width.derivative(t)
        dd = points[:, 2] * self.height.derivative(t) + 2 * self.fold_per_m * q * dq
        position = centre + q[:, None] * a + d[:, None] * b
        dx = width[:, None] * (a + (2 * self.fold_per_m * q)[:, None] * b)
        dy = (
            tangent
            + (dq[:, None] * a + q[:, None] * da + dd[:, None] * b + d[:, None] * db)
            / self.length
        )
        dz = height[:, None] * b
        return position, np.stack((dx, dy, dz), axis=-1)

    def deform(self, mesh, method="batch"):
        vertices, faces, normals = mesh["vertices"], mesh["faces"], mesh["normals"]
        if (
            method not in ("batch", "scalar")
            or np.iscomplexobj(vertices)
            or np.iscomplexobj(normals)
            or vertices.ndim != 2
            or vertices.shape[1:] != (3,)
            or faces.ndim != 2
            or faces.shape[1:] != (3,)
            or normals.shape != (faces.size, 3)
            or len(vertices) > MAX_VERTICES
            or len(faces) > MAX_TRIANGLES
        ):
            raise ValueError(
                "bounded complete frond mesh and declared evaluator required"
            )
        if not np.issubdtype(faces.dtype, np.integer) or np.any(
            (faces < 0) | (faces >= len(vertices))
        ):
            raise ValueError("valid source triangle indices required")
        if not np.isfinite(normals).all() or np.any(
            np.linalg.norm(normals, axis=1) == 0
        ):
            raise ValueError("finite nonzero source corner normals required")
        self.validate(vertices)
        if method == "batch" or not len(vertices):
            positions, jacobian = self.evaluate(vertices)
        else:
            samples = [self.evaluate(point[None]) for point in vertices]
            positions = np.concatenate([sample[0] for sample in samples])
            jacobian = np.concatenate([sample[1] for sample in samples])
        columns = jacobian.transpose(0, 2, 1)
        cofactor = np.stack(
            (
                np.cross(columns[:, 1], columns[:, 2]),
                np.cross(columns[:, 2], columns[:, 0]),
                np.cross(columns[:, 0], columns[:, 1]),
            ),
            axis=-1,
        )
        mapped = np.einsum("nij,nj->ni", cofactor[faces.ravel()], normals)
        mapped /= np.linalg.norm(mapped, axis=1, keepdims=True)
        if not np.isfinite(positions).all() or not np.isfinite(mapped).all():
            raise ValueError("finite deformed frond product required")
        return dict(mesh) | {"vertices": positions, "normals": mapped}
