"""One closed metrical wall panel with an actual rectangular through opening."""

import math
from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True, slots=True)
class OpeningPanel:
    width_m: float
    height_m: float
    opening: tuple[float, float, float, float]
    depth_m: float = 0.24

    def __post_init__(self):
        if not isinstance(self.opening, tuple) or len(self.opening) != 4:
            raise ValueError("Immutable local (u0,v0,u1,v1) opening required")
        values = (self.width_m, self.height_m, self.depth_m, *self.opening)
        if any(isinstance(v, (bool, str)) or not math.isfinite(v) for v in values):
            raise ValueError("Finite metric panel dimensions required")
        x0, y0, x1, y1 = self.opening
        if not (
            self.depth_m > 0
            and 0 < x0 < x1 < self.width_m
            and 0 < y0 < y1 < self.height_m
        ):
            raise ValueError(
                "Opening must leave positive wall margins on all four sides"
            )


def panel_surface(plan, *, max_triangles=32):
    if not isinstance(plan, OpeningPanel):
        raise TypeError("An OpeningPanel is required")
    if type(max_triangles) is not int or max_triangles < 32:
        raise ValueError("Opening panel exceeds triangle capacity")
    x0, y0, x1, y1 = plan.opening
    outer = np.array(
        [
            [0.0, 0.0],
            [plan.width_m, 0.0],
            [plan.width_m, plan.height_m],
            [0.0, plan.height_m],
        ]
    )
    inner = np.array([[x0, y0], [x1, y0], [x1, y1], [x0, y1]])
    quads, roles = [], []
    for depth, reverse, role in ((0.0, False, 0), (-plan.depth_m, True, 1)):
        for i in range(4):
            j = (i + 1) % 4
            points = np.c_[[outer[i], outer[j], inner[j], inner[i]], np.full(4, depth)]
            quads.append(points[::-1] if reverse else points)
            roles.append(role)
    for ring, reverse, role in ((outer, True, 2), (inner, False, 3)):
        for i in range(4):
            j = (i + 1) % 4
            points = np.array(
                [
                    [*ring[i], 0.0],
                    [*ring[j], 0.0],
                    [*ring[j], -plan.depth_m],
                    [*ring[i], -plan.depth_m],
                ]
            )
            quads.append(points[::-1] if reverse else points)
            roles.append(role)
    quads = np.asarray(quads)
    normals = np.cross(quads[:, 1] - quads[:, 0], quads[:, 2] - quads[:, 0])
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    uv = np.empty((len(quads), 4, 2))
    for i, quad in enumerate(quads):
        axes = np.flatnonzero(np.ptp(quad, axis=0) > 0)
        uv[i] = quad[:, axes]
    return {
        "vertices": quads.reshape(-1, 3),
        "faces": np.arange(len(quads) * 4, dtype=np.uint32)
        .reshape(-1, 4)[:, [[0, 1, 2], [0, 2, 3]]]
        .reshape(-1, 3),
        "normals": np.repeat(normals, 4, axis=0),
        "uv": uv.reshape(-1, 2),
        "roles": np.repeat(np.asarray(roles, dtype=np.uint8), 2),
    }
