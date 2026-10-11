"""Metre-scale cupped leaves and folded grass ribbons with analytical normals."""

from dataclasses import dataclass
from functools import lru_cache

import numpy as np
from curve_frame import arc_frame


@lru_cache(maxsize=16)
def quadrature(order):
    nodes, weights = np.polynomial.legendre.leggauss(order)
    nodes.setflags(write=False)
    weights.setflags(write=False)
    return nodes, weights


@dataclass(frozen=True)
class Blade:
    length: float = 0.12
    width: float = 0.06
    widest: float = 0.62
    lobes: int = 4
    lobe_depth: float = 0.35
    base_fill: float = 0.03
    bend: float = 0.32
    fold: float = 0.13
    fold_power: int = 2
    twist: float = 0.1
    petiole: float = 0.004

    def __post_init__(self):
        if not all(
            np.isfinite(v)
            for v in (
                self.length,
                self.width,
                self.widest,
                self.lobe_depth,
                self.base_fill,
                self.bend,
                self.fold,
                self.twist,
                self.petiole,
            )
        ):
            raise ValueError("finite blade parameters required")
        if self.length <= 0 or self.width <= 0 or not 0 < self.widest < 1:
            raise ValueError(
                "positive blade dimensions and interior width maximum required"
            )
        if not isinstance(self.lobes, (int, np.integer)) or not 0 <= self.lobes <= 32:
            raise ValueError("bounded nonnegative integer lobe count required")
        if not isinstance(
            self.fold_power, (int, np.integer)
        ) or self.fold_power not in (1, 2):
            raise ValueError("linear fold or quadratic cup required")
        if (
            not 0 <= self.lobe_depth < 1
            or not 0 <= self.base_fill <= 1
            or self.petiole < 0
        ):
            raise ValueError("valid margin, base and petiole required")
        if (
            abs(self.bend) > np.pi
            or abs(self.bend) * self.width / 2 * (1 + abs(self.fold)) >= self.length
        ):
            raise ValueError("blade curvature must preserve a regular ribbon")

    def width_profile(self, t):
        p = self.widest
        denominator = p + (1 - 2 * p) * t
        warped = (1 - p) * t / denominator
        derivative = p * (1 - p) / denominator**2
        base = (1 - self.base_fill) * np.sin(np.pi * warped) + self.base_fill * (1 - t)
        base = np.where(t == 1, 0.0, base)
        base_derivative = (1 - self.base_fill) * np.pi * np.cos(
            np.pi * warped
        ) * derivative - self.base_fill
        phase = np.pi * self.lobes * (warped - 0.5)
        margin = 1 - self.lobe_depth * np.sin(phase) ** 2
        margin_derivative = (
            -self.lobe_depth * np.pi * self.lobes * np.sin(2 * phase) * derivative
        )
        return self.width / 2 * base * margin, self.width / 2 * (
            base_derivative * margin + base * margin_derivative
        )

    def evaluate(self, t, s, side, petiole_side=False):
        t, s, side, petiole_side = np.broadcast_arrays(
            np.asarray(t, dtype=np.float64),
            np.asarray(s, dtype=np.float64),
            np.asarray(side),
            np.asarray(petiole_side, dtype=bool),
        )
        if (
            not np.all(np.isfinite(t) & np.isfinite(s))
            or np.any((t < -self.petiole / self.length) | (t > 1))
            or np.any(np.abs(s) > 1)
            or np.any((np.abs(side) != 1) | (s * side < 0))
            or np.any(petiole_side & (t > 0))
        ):
            raise ValueError(
                "valid blade coordinates and one-sided derivatives required"
            )
        petiole = (t < 0) | petiole_side
        positive = np.maximum(t, 0)
        centre, tangent, lateral, normal = arc_frame(
            self.length, self.bend, self.twist, positive
        )
        centre[..., 1] += np.where(petiole, t * self.length, 0)
        width, derivative = self.width_profile(positive)
        derivative = np.where(petiole, 0.0, derivative)
        bend, twist = (
            np.where(petiole, 0.0, self.bend),
            np.where(petiole, 0.0, self.twist),
        )
        dt_lateral = (
            twist[..., None] * normal
            - (bend * np.sin(self.twist * positive))[..., None] * tangent
        )
        dt_normal = (
            -twist[..., None] * lateral
            - (bend * np.cos(self.twist * positive))[..., None] * tangent
        )
        profile = np.abs(s) ** self.fold_power
        slope = self.fold_power * np.abs(s) ** (self.fold_power - 1) * side
        position = (
            centre
            + (s * width)[..., None] * lateral
            + (profile * width * self.fold)[..., None] * normal
        )
        dt = (
            self.length * tangent
            + (s * derivative)[..., None] * lateral
            + (s * width)[..., None] * dt_lateral
        )
        dt += (profile * self.fold * derivative)[..., None] * normal + (
            profile * self.fold * width
        )[..., None] * dt_normal
        ds = lateral + (self.fold * slope)[..., None] * normal
        normals = np.cross(ds, dt)
        magnitude = np.linalg.norm(normals, axis=-1)
        normals /= magnitude[..., None]
        return position, normals, width * magnitude

    def mesh(self, segments=32, across=1):
        if not isinstance(segments, (int, np.integer)) or not 2 <= segments <= 1024:
            raise ValueError(
                "integer blade segment count between two and 1024 required"
            )
        if not isinstance(across, (int, np.integer)) or not 1 <= across <= 32:
            raise ValueError("transverse segment count between one and 32 required")
        t = np.linspace(0, 1, segments + 1)
        if self.petiole > 0 and self.base_fill > 0:
            t = np.insert(t, 0, -self.petiole / self.length)
        u, v = np.meshgrid(t, np.linspace(-1, 1, 2 * across + 1), indexing="ij")
        positions, _, _ = self.evaluate(u, v, np.where(v < 0, -1, 1))
        indices = np.arange(positions.size // 3).reshape(-1, 2 * across + 1)
        indices[-1] = indices[-1, across]
        if self.base_fill == 0:
            indices[0] = indices[0, across]
        faces, sides = [], []
        for row in range(len(t) - 1):
            for col in range(2 * across):
                side = -1 if col < across else 1
                a, b = indices[row, col : col + 2]
                d, c = indices[row + 1, col : col + 2]
                for triangle in ((a, b, c), (a, c, d)):
                    if len(set(triangle)) == 3:
                        faces.append(triangle)
                        sides.append(side)
        faces = np.asarray(faces, dtype=np.int32)
        used, inverse = np.unique(faces, return_inverse=True)
        normals = self.evaluate(
            u.reshape(-1)[faces],
            v.reshape(-1)[faces],
            np.asarray(sides)[:, None],
            petiole_side=np.any(u.reshape(-1)[faces] < 0, axis=1)[:, None],
        )[1]
        return {
            "vertices": positions.reshape(-1, 3)[used],
            "faces": inverse.reshape(-1, 3).astype(np.int32),
            "normals": normals.reshape(-1, 3),
            "parameters": np.column_stack((u.reshape(-1)[used], v.reshape(-1)[used])),
        }

    def area(self, order=128):
        if not isinstance(order, (int, np.integer)) or not 8 <= order <= 1024:
            raise ValueError("quadrature order between eight and 1024 required")
        nodes, weights = quadrature(order)
        across, across_weights = quadrature(12)
        t = (nodes + 1) / 2
        total = 0.0
        for side in (-1, 1):
            s = side * (across + 1) / 2
            u, v = np.meshgrid(t, s, indexing="ij")
            surface = self.evaluate(u, v, side)[2]
            total += float(
                np.sum(surface * weights[:, None] * across_weights[None, :]) / 4
            )
        if self.petiole > 0 and self.base_fill > 0:
            width = self.width_profile(np.array([0.0]))[0][0]
            slope = self.fold_power * ((across + 1) / 2) ** (self.fold_power - 1)
            cross_length = np.sum(
                across_weights * np.sqrt(1 + (self.fold * slope) ** 2)
            )
            total += width * self.petiole * cross_length
        return total
