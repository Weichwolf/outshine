"""Closed, pointed needles with elliptical or hard rhombic swept sections."""

from dataclasses import asdict, dataclass

import numpy as np
from curve_frame import arc_frame


@dataclass(frozen=True)
class Needle:
    length: float = 0.055
    radius: float = 0.0007
    aspect: float = 0.65
    bend: float = 0.25
    twist: float = 0.5
    taper_start: float = 0.75
    section: str = "ellipse"

    def __post_init__(self):
        if not all(
            np.isfinite(v)
            for v in (
                self.length,
                self.radius,
                self.aspect,
                self.bend,
                self.twist,
                self.taper_start,
            )
        ):
            raise ValueError("finite physical needle parameters required")
        if not 0 < self.radius <= self.length / 8 or not 0.125 <= self.aspect <= 1:
            raise ValueError(
                "slender positive needle and bounded section aspect required"
            )
        if (
            abs(self.bend) > np.pi
            or abs(self.twist) > 4 * np.pi
            or not 0.25 <= self.taper_start <= 0.875
        ):
            raise ValueError("bounded circular bending, twist and taper required")
        if self.section not in ("ellipse", "rhombus"):
            raise ValueError("implemented closed needle section required")

    def recipe(self):
        return asdict(self)

    def radial_profile(self, t):
        t = np.asarray(t, float)
        q = np.minimum(t / self.taper_start, 1)
        smooth = q * q * (3 - 2 * q)
        body = 0.55 + 0.45 * smooth
        derivative = 0.45 * 6 * q * (1 - q) / self.taper_start
        end = (t - self.taper_start) / (1 - self.taper_start)
        radius = np.where(t <= self.taper_start, body, 1 - end**2) * self.radius
        dr = (
            np.where(
                t <= self.taper_start, derivative, -2 * end / (1 - self.taper_start)
            )
            * self.radius
        )
        return radius, dr

    def section_field(self, angle, edge=None):
        angle = np.asarray(angle, float)
        if self.section == "ellipse":
            point = np.stack((np.cos(angle), self.aspect * np.sin(angle)), axis=-1)
            derivative = np.stack(
                (-np.sin(angle), self.aspect * np.cos(angle)), axis=-1
            )
            return point, derivative
        coordinate = np.remainder(angle, 2 * np.pi) / (np.pi / 2)
        index = (
            np.floor(coordinate).astype(int)
            if edge is None
            else np.asarray(edge, int) % 4
        )
        corners = np.array(((1, 0), (0, self.aspect), (-1, 0), (0, -self.aspect)))
        following = corners[(index + 1) % 4] - corners[index]
        fraction = np.remainder(coordinate - index, 4)
        return corners[index] + fraction[..., None] * following, following / (np.pi / 2)

    def evaluate(self, t, angle, edge=None):
        t, angle = np.broadcast_arrays(np.asarray(t, float), np.asarray(angle, float))
        if not np.isfinite([t, angle]).all() or np.any((t < 0) | (t > 1)):
            raise ValueError("bounded finite needle coordinates required")
        centre, tangent, lateral, normal = arc_frame(
            self.length, self.bend, self.twist, t
        )
        section, ds = self.section_field(angle, edge)
        radius, dr = self.radial_profile(t)
        offset = section[..., 0, None] * lateral + section[..., 1, None] * normal
        dl = (
            self.twist * normal
            - (self.bend * np.sin(self.twist * t))[..., None] * tangent
        )
        dn = (
            -self.twist * lateral
            - (self.bend * np.cos(self.twist * t))[..., None] * tangent
        )
        dt = (
            self.length * tangent
            + dr[..., None] * offset
            + radius[..., None]
            * (section[..., 0, None] * dl + section[..., 1, None] * dn)
        )
        angular = ds[..., 0, None] * lateral + ds[..., 1, None] * normal
        cross = np.cross(dt, angular)
        magnitude = np.linalg.norm(cross, axis=-1)
        return (
            centre + radius[..., None] * offset,
            cross / magnitude[..., None],
            radius * magnitude,
        )

    def mesh(self, segments=32, sides=16, method="batch"):
        if (
            type(segments) is not int
            or not 2 <= segments <= 128
            or type(sides) is not int
            or sides not in (4, 8, 16, 32)
        ):
            raise ValueError("bounded nested needle subdivisions required")
        if method not in ("batch", "scalar"):
            raise ValueError("declared differential evaluation method required")
        sides = 4 if self.section == "rhombus" else sides
        t, angle = np.meshgrid(
            np.arange(segments) / segments,
            np.arange(sides) * 2 * np.pi / sides,
            indexing="ij",
        )
        parameters = np.column_stack((t.ravel(), angle.ravel()))
        vertices = self.evaluate(parameters[:, 0], parameters[:, 1])[0]
        centre, tangent, _, _ = arc_frame(
            self.length, self.bend, self.twist, np.array([0.0, 1.0])
        )
        vertices = np.vstack((vertices, centre))
        base, tip = len(vertices) - 2, len(vertices) - 1
        ring = np.arange(segments * sides).reshape(segments, sides)
        a, b = ring[:-1], np.roll(ring[:-1], -1, axis=1)
        d, c = ring[1:], np.roll(ring[1:], -1, axis=1)
        faces = np.stack(
            (np.stack((a, d, c), axis=-1), np.stack((a, c, b), axis=-1)), axis=2
        ).reshape(-1, 3)
        top = np.column_stack((ring[-1], np.full(sides, tip), np.roll(ring[-1], -1)))
        bottom = np.column_stack((np.full(sides, base), ring[0], np.roll(ring[0], -1)))
        faces = np.vstack((faces, top, bottom)).astype(np.int32)
        parameters = np.vstack((parameters, [[0, 0], [1, 0]]))
        corners = parameters[faces].copy()
        edges = np.concatenate(
            (
                np.tile(np.repeat(np.arange(sides), 2), segments - 1),
                np.arange(sides),
                np.arange(sides),
            )
        )
        corners[np.any(faces == tip, axis=1), 1, 1] = (
            (np.arange(sides) + 0.5) * 2 * np.pi / sides
        )
        if self.section == "rhombus":
            query_edges = np.broadcast_to(edges[:, None], faces.shape).ravel()
        else:
            query_edges = None
        query = corners.reshape(-1, 2)
        if method == "batch":
            normals = self.evaluate(query[:, 0], query[:, 1], query_edges)[1]
        else:
            normals = np.array(
                [
                    self.evaluate(
                        p[0], p[1], None if query_edges is None else query_edges[i]
                    )[1]
                    for i, p in enumerate(query)
                ]
            )
        normals[-3 * sides :] = np.tile(-tangent[0], (3 * sides, 1))
        uv = np.column_stack((parameters[:, 1] / (2 * np.pi), parameters[:, 0]))
        corner_uv = uv[faces].copy()
        corner_uv[..., 0] = corners[..., 1] / (2 * np.pi)
        seam = corner_uv[..., 0].max(axis=1) - corner_uv[..., 0].min(axis=1) > 0.5
        corner_uv[seam, :, 0] = np.where(
            corner_uv[seam, :, 0] < 0.5,
            corner_uv[seam, :, 0] + 1,
            corner_uv[seam, :, 0],
        )
        return {
            "vertices": vertices,
            "faces": faces,
            "normals": normals,
            "parameters": parameters,
            "uv": uv,
            "corner_uv": corner_uv.reshape(-1, 2),
        }
