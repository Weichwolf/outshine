"""Closed pointed organs in a right-handed X-cross/Y-axis/Z-normal metre frame."""

from dataclasses import asdict, dataclass

import numpy as np

from .curve_frame import arc_frame
from .needle_sampling import axial_parameters
from .needle_section import section_centroid, section_field, section_sampling


@dataclass(frozen=True)
class Needle:
    length: float = 0.055
    radius: float = 0.0007
    aspect: float = 0.65
    bend: float = 0.25
    twist: float = 0.5
    taper_start: float = 0.75
    section: str = "ellipse"
    sector_count: int = 2

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
        if self.section not in ("ellipse", "rhombus", "sector"):
            raise ValueError("implemented closed needle section required")
        if type(self.sector_count) is not int or not 2 <= self.sector_count <= 8:
            raise ValueError("two to eight sector divisions required")

    def recipe(self):
        recipe = asdict(self)
        if self.section != "sector":
            recipe.pop("sector_count")
        return recipe

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
        return section_field(self.section, self.aspect, self.sector_count, angle, edge)

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

    def mesh(self, segments=32, sides=16, method="batch", *, axial_sampling="uniform"):
        """Return owned mesh buffers; reject underresolved faces before publication.

        Positions/UVs/parameters and per-corner normals are float64, faces int32.
        Hard section edges and caps have independent corner normals/UVs.
        Subdivision controls approximation only; it is not a screen-error bound.
        Profile sampling retains the widest ring and divides segments between
        the body and pointed tip; uniform sampling preserves previous buffers.
        """
        if (
            type(segments) is not int
            or not 2 <= segments <= 128
            or type(sides) is not int
            or sides not in (4, 8, 16, 32)
        ):
            raise ValueError("bounded nested needle subdivisions required")
        if method not in ("batch", "scalar"):
            raise ValueError("declared differential evaluation method required")
        angles, middle, tags = section_sampling(self.section, sides)
        sides = len(angles)
        axial = axial_parameters(segments, self.taper_start, axial_sampling)
        t, angle = np.meshgrid(axial, angles, indexing="ij")
        parameters = np.column_stack((t.ravel(), angle.ravel()))
        vertices = self.evaluate(parameters[:, 0], parameters[:, 1])[0]
        centre, tangent, _, _ = arc_frame(
            self.length, self.bend, self.twist, np.array([0.0, 1.0])
        )
        centroid = section_centroid(self.section, self.aspect, self.sector_count)
        radius = self.radial_profile(0)[0]
        centre[0] += radius * np.array([centroid[0], 0, centroid[1]])
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
        corners[np.any(faces == tip, axis=1), 1, 1] = middle
        if tags is not None:
            query_edges = np.broadcast_to(
                tags[edges[:-sides], None], faces[:-sides].shape
            ).ravel()
        else:
            query_edges = None
        query = corners[:-sides].reshape(-1, 2)
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
        normals = np.vstack((normals, np.tile(-tangent[0], (3 * sides, 1))))
        uv = np.column_stack((parameters[:, 1] / (2 * np.pi), parameters[:, 0]))
        corner_uv = uv[faces].copy()
        corner_uv[..., 0] = corners[..., 1] / (2 * np.pi)
        seam = corner_uv[..., 0].max(axis=1) - corner_uv[..., 0].min(axis=1) > 0.5
        corner_uv[seam, :, 0] = np.where(
            corner_uv[seam, :, 0] < 0.5,
            corner_uv[seam, :, 0] + 1,
            corner_uv[seam, :, 0],
        )
        result = {
            "vertices": vertices,
            "faces": faces,
            "normals": normals,
            "parameters": parameters,
            "uv": uv,
            "corner_uv": corner_uv.reshape(-1, 2),
        }

        _validate_mesh(result)
        return result


def _validate_mesh(mesh):
    p = mesh["vertices"][mesh["faces"]]
    cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    normals = mesh["normals"].reshape(-1, 3, 3)
    if not all(np.isfinite(a).all() for a in mesh.values()) or np.any(
        np.sum(cross[:, None] * normals, axis=2) <= 0
    ):
        raise ValueError("needle subdivision does not resolve the oriented surface")
