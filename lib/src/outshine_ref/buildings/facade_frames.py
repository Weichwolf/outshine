"""One joined metrical facade lattice, with shared physical and outer products."""

from dataclasses import dataclass
from itertools import pairwise

import numpy as np
import shapely

from .modelling.faces import Face, compile_faces
from .modelling.scope import scalar


def positions(values, extent, thickness):
    if not isinstance(values, tuple) or not 2 <= len(values) <= 33:
        raise ValueError("Frame axes require an immutable tuple of 2..33 centres")
    coordinates = np.asarray([scalar(v) for v in values])
    tolerance = 16 * np.finfo(float).eps * max(1.0, extent)
    if (
        abs(coordinates[0] - thickness / 2) > tolerance
        or abs(coordinates[-1] - (extent - thickness / 2)) > tolerance
        or np.any(np.diff(coordinates) <= thickness)
    ):
        raise ValueError(
            "Frame bars need outer support and strictly positive apertures"
        )
    return coordinates


@dataclass(frozen=True, slots=True)
class FacadeFrame:
    width_m: float
    height_m: float
    vertical: tuple
    horizontal: tuple
    pier_m: float = 0.24
    beam_m: float = 0.22
    depth_m: float = 0.20

    def __post_init__(self):
        for name in ("width_m", "height_m", "pier_m", "beam_m", "depth_m"):
            value = scalar(getattr(self, name))
            if value <= 0:
                raise ValueError("Frame dimensions require positive finite metres")
            object.__setattr__(self, name, value)
        positions(self.vertical, self.width_m, self.pier_m)
        positions(self.horizontal, self.height_m, self.beam_m)
        if self.apertures > 256:
            raise ValueError("Frame exceeds 256 rectangular apertures")

    @property
    def apertures(self):
        return (len(self.vertical) - 1) * (len(self.horizontal) - 1)

    def triangle_count(self, back_covered=False):
        return (
            (10 + 14 * self.apertures) if back_covered else (12 + 20 * self.apertures)
        )


def region(plan):
    if not isinstance(plan, FacadeFrame):
        raise TypeError("A FacadeFrame is required")
    u, v = np.asarray(plan.vertical), np.asarray(plan.horizontal)
    x0, x1 = u[:-1] + plan.pier_m / 2, u[1:] - plan.pier_m / 2
    y0, y1 = v[:-1] + plan.beam_m / 2, v[1:] - plan.beam_m / 2
    holes = np.empty((len(x0), len(y0), 4, 2))
    for i, (x, y) in enumerate(((x0, y0), (x0, y1), (x1, y1), (x1, y0))):
        holes[:, :, i, 0] = x[:, None]
        holes[:, :, i, 1] = y[None, :]
    outer = (
        (0.0, 0.0),
        (plan.width_m, 0.0),
        (plan.width_m, plan.height_m),
        (0.0, plan.height_m),
    )
    return shapely.polygons(outer, holes=holes.reshape(-1, 4, 2))


def front(polygon):
    vertices, rings = [], []
    for ring in (polygon.exterior, *polygon.interiors):
        points = np.asarray(ring.coords)[:-1]
        rings.append(tuple(range(len(vertices), len(vertices) + len(points))))
        vertices.extend(np.c_[points, np.zeros(len(points))])
    mesh, _ = compile_faces(np.asarray(vertices), [Face(rings[0], tuple(rings[1:]))])
    return mesh["vertices"][mesh["faces"]]


def sides(polygon, depth):
    quads, coordinates = [], []
    for ring in (polygon.exterior, *polygon.interiors):
        points = np.asarray(ring.coords)
        for a, b in pairwise(points):
            quads.append([[*a, 0], [*b, 0], [*b, depth], [*a, depth]])
            width = float(np.linalg.norm(b - a))
            coordinates.append([[0, 0], [width, 0], [width, depth], [0, depth]])
    order = np.array([[0, 1, 2], [0, 2, 3]])
    return np.asarray(quads)[:, order].reshape(-1, 3, 3), np.asarray(coordinates)[
        :, order
    ].reshape(-1, 3, 2)


def frame_surface(plan, *, back_covered=False, material=1, max_triangles=16384):
    """Local (U,V,outward) metres. Covered backs are never materialized."""
    if not isinstance(plan, FacadeFrame):
        raise TypeError("A FacadeFrame is required")
    if not isinstance(back_covered, bool):
        raise TypeError("Covered-back selection requires boolean ownership")
    if type(max_triangles) is not int or max_triangles < plan.triangle_count(
        back_covered
    ):
        raise ValueError("Frame triangle capacity exceeded")
    if (
        isinstance(material, bool)
        or not isinstance(material, (int, np.integer))
        or not 0 <= material <= 65535
    ):
        raise ValueError("Frame material requires an unsigned 16-bit identifier")
    polygon = region(plan)
    upper = front(polygon)
    if len(upper) != 2 + 6 * plan.apertures:
        raise ValueError(
            "Frame triangulation changed its rectangular aperture topology"
        )
    upper[:, :, 2] = plan.depth_m
    walls, wall_uv = sides(polygon, plan.depth_m)
    pieces, uv = [upper, walls], [upper[:, :, :2], wall_uv]
    roles = [
        np.zeros(len(upper), dtype=np.uint8),
        np.full(len(walls), 2, dtype=np.uint8),
    ]
    if not back_covered:
        lower = upper[:, ::-1].copy()
        lower[:, :, 2] = 0
        pieces.append(lower)
        uv.append(lower[:, :, :2])
        roles.append(np.ones(len(lower), dtype=np.uint8))
    triangles = np.concatenate(pieces)
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    lengths = np.linalg.norm(normals, axis=1)
    if np.any(lengths == 0) or not np.isfinite(triangles).all():
        raise ValueError("Frame produced unresolved geometry")
    normals /= lengths[:, None]
    return {
        "vertices": triangles.reshape(-1, 3),
        "faces": np.arange(triangles.size // 3, dtype=np.uint32).reshape(-1, 3),
        "normals": np.repeat(normals, 3, axis=0),
        "uv": np.concatenate(uv).reshape(-1, 2),
        "materials": np.full(len(triangles), material, dtype=np.uint16),
        "roles": np.concatenate(roles),
    }
