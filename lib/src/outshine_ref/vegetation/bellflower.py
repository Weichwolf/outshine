"""Own thin pendulous six-lobed corolla surface; closed attachment, open mouth."""

from dataclasses import dataclass
from numbers import Real

import numpy as np


@dataclass(frozen=True)
class Bellflower:
    length: float = 0.0075
    radius: float = 0.0035
    lobes: int = 6

    def __post_init__(self):
        if (
            not all(isinstance(value, Real) for value in (self.length, self.radius))
            or not np.isfinite((self.length, self.radius)).all()
            or min(self.length, self.radius) < 1e-6
            or max(self.length, self.radius) > 10
        ):
            raise ValueError(
                "finite corolla dimensions within1micrometre..10metres required"
            )
        if type(self.lobes) is not int or not 3 <= self.lobes <= 12:
            raise ValueError("three to12 corolla lobes required")

    def surface(self, t, angle):
        t, angle = np.broadcast_arrays(np.asarray(t, float), np.asarray(angle, float))
        if (
            not np.isfinite((t, angle)).all()
            or np.any((t < 0) | (t > 1))
            or np.any(abs(angle) > 16 * np.pi)
        ):
            raise ValueError("finite corolla angle and longitudinal0..1 required")
        phase = self.lobes * angle
        lobe = 0.5 + 0.5 * np.cos(phase)
        dlobe = -0.5 * self.lobes * np.sin(phase)
        r = self.radius * (0.16 + 0.84 * np.sin(0.85 * np.pi * t) + 0.15 * t**12 * lobe)
        rt = self.radius * (
            0.84 * 0.85 * np.pi * np.cos(0.85 * np.pi * t) + 1.8 * t**11 * lobe
        )
        ra = self.radius * 0.15 * t**12 * dlobe
        z = self.length * (-t + 0.06 * t**12 * (1 - lobe))
        zt = self.length * (-1 + 0.72 * t**11 * (1 - lobe))
        za = -self.length * 0.06 * t**12 * dlobe
        c, s = np.cos(angle), np.sin(angle)
        p = np.stack((r * c, r * s, z), axis=-1)
        dt = np.stack((rt * c, rt * s, zt), axis=-1)
        da = np.stack((ra * c - r * s, ra * s + r * c, za), axis=-1)
        n = np.cross(dt, da)
        n /= np.linalg.norm(n, axis=-1, keepdims=True)
        return p, n, dt, da

    def mesh(self, segments=6, sides=24):
        if (
            type(segments) is not int
            or not 3 <= segments <= 128
            or type(sides) is not int
            or not 3 * self.lobes <= sides <= 256
        ):
            raise ValueError(
                "bounded corolla subdivisions and at least three samples/lobe required"
            )
        t, angle = np.meshgrid(
            np.linspace(0, 1, segments + 1),
            np.arange(sides) * 2 * np.pi / sides,
            indexing="ij",
        )
        p, normals, _, _ = self.surface(t, angle)
        a = (np.arange(segments)[:, None] * sides + np.arange(sides)).ravel()
        b = (a // sides) * sides + (a + 1) % sides
        faces = np.concatenate((np.c_[a, a + sides, b], np.c_[b, a + sides, b + sides]))
        n = normals.reshape(-1, 3)[faces.ravel()]
        uv = np.c_[t.ravel(), angle.ravel() / (2 * np.pi)][faces.ravel()]
        corner_u = uv[:, 1].reshape(-1, 3)
        seam = np.ptp(corner_u, axis=1) > 0.5
        corner_u[seam] = np.where(
            corner_u[seam] < 0.5, corner_u[seam] + 1, corner_u[seam]
        )
        vertices = np.vstack((p.reshape(-1, 3), np.zeros(3)))
        cap = np.c_[
            np.full(sides, len(vertices) - 1),
            np.arange(sides),
            (np.arange(sides) + 1) % sides,
        ]
        result = {
            "vertices": vertices,
            "faces": np.vstack((faces, cap)).astype(np.int32),
            "normals": np.vstack((n, np.tile((0, 0, 1), (3 * sides, 1)))),
            "corner_uv": np.vstack(
                (uv, vertices[cap.ravel(), :2] / (2 * self.radius) + 0.5)
            ),
        }
        points = vertices[result["faces"]]
        geometric = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
        corners = result["normals"].reshape(-1, 3, 3)
        if not np.all(np.einsum("ij,ikj->ik", geometric, corners) > 0):
            raise ValueError(
                "subdivision does not resolve the oriented corolla surface"
            )
        return result
