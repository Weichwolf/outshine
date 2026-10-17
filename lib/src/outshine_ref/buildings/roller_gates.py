"""Bounded visible roller leaves; definition, lift and unmodelled winding stay separate."""

import math
from dataclasses import dataclass

import numpy as np
import shapely

from .facade_frames import front
from .modelling.scope import scalar


@dataclass(frozen=True, slots=True)
class RollerGate:
    width_m: float
    height_m: float
    pitch_m: float = 0.15
    depth_m: float = 0.035
    rib_m: float = 0.006

    def __post_init__(self):
        for key in ("width_m", "height_m", "pitch_m", "depth_m", "rib_m"):
            object.__setattr__(self, key, scalar(getattr(self, key)))
        if not (
            0.5 <= self.width_m <= 12
            and 0.5 <= self.height_m <= 4.5
            and 0.03 <= self.pitch_m <= self.height_m
            and 0.005 <= self.depth_m <= 0.1
            and 0 <= self.rib_m <= self.depth_m
            and math.ceil(self.height_m / self.pitch_m) <= 32
        ):
            raise ValueError("Roller dimensions or 32-slat capacity exceeded")


def lift(gate, opened):
    if not isinstance(gate, RollerGate):
        raise TypeError("A RollerGate is required")
    value = scalar(opened)
    if not 0 <= value <= 1:
        raise ValueError("Roller opening fraction must lie in [0,1]")
    lifted = gate.height_m * value
    return gate.height_m if gate.height_m - lifted <= 1e-7 else lifted


def profile(gate, opened, detail=True):
    bottom = lift(gate, opened)
    length = gate.height_m - bottom
    if length == 0:
        return np.empty((0, 2))
    if not isinstance(detail, bool):
        raise TypeError("Roller detail selection must be boolean")
    if not detail or gate.rib_m == 0:
        return np.array([[bottom, gate.depth_m], [gate.height_m, gate.depth_m]])
    count = math.ceil(length / gate.pitch_m)
    positions = np.r_[
        0,
        np.column_stack((np.arange(count) + 0.8, np.arange(count) + 1)).ravel()
        * gate.pitch_m,
    ]
    tolerance = 32 * np.finfo(float).eps * max(1.0, gate.height_m)
    positions = np.r_[positions[positions < length - tolerance], length]
    phase = (positions / gate.pitch_m) % 1
    depth = gate.depth_m + gate.rib_m * np.where(
        phase <= 0.8, phase / 0.8, (1 - phase) / 0.2
    )
    return np.c_[positions + bottom, depth]


def extrusion(section, width):
    polygon = shapely.Polygon([(section[0, 0], 0), (section[-1, 0], 0), *section[::-1]])
    caps = front(polygon)[:, :, :2]
    first = np.concatenate((np.zeros((*caps.shape[:2], 1)), caps), axis=2)
    last = first.copy()
    last[:, :, 0] = width
    ring = np.asarray(polygon.exterior.coords)
    a, b = ring[:-1], ring[1:]
    quads = np.stack(
        (
            np.c_[np.zeros(len(a)), a],
            np.c_[np.zeros(len(b)), b],
            np.c_[np.full(len(b), width), b],
            np.c_[np.full(len(a), width), a],
        ),
        axis=1,
    )
    sides = quads[:, [[0, 1, 2], [0, 2, 3]]].reshape(-1, 3, 3)
    return np.concatenate((first[:, ::-1], last, sides))


def pack(triangles, section, material):
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    lengths = np.linalg.norm(normals, axis=1)
    if not np.isfinite(triangles).all() or np.any(lengths == 0):
        raise ValueError("Roller leaf produced unresolved triangles")
    normals /= lengths[:, None]
    uv = triangles[:, :, :2].copy()
    uv[:, :, 1] -= section[0, 0]
    side = abs(normals[:, 0]) > 0.999999
    uv[side] = triangles[side, :, 1:3]
    cap = abs(normals[:, 1]) > 0.999999
    uv[cap] = triangles[cap][:, :, [0, 2]]
    outer = (normals[:, 2] > 1e-8) & ~side & ~cap
    run = np.r_[0, np.cumsum(np.linalg.norm(np.diff(section, axis=0), axis=1))]
    uv[outer, :, 1] = np.interp(triangles[outer, :, 1], section[:, 0], run)
    return {
        "vertices": triangles.reshape(-1, 3),
        "faces": np.arange(triangles.size // 3, dtype=np.uint32).reshape(-1, 3),
        "normals": np.repeat(normals, 3, axis=0),
        "uv": uv.reshape(-1, 2),
        "materials": np.full(len(triangles), material, dtype=np.uint16),
    }


def leaf_surface(gate, opened=0.0, *, detail=True, material=10, max_triangles=512):
    bottom = lift(gate, opened)
    if not isinstance(detail, bool):
        raise TypeError("Roller detail selection must be boolean")
    detail = detail and gate.rib_m > 0
    if (
        isinstance(material, bool)
        or not isinstance(material, (int, np.integer))
        or not 0 <= material <= 65535
    ):
        raise ValueError("Roller material requires an unsigned 16-bit identifier")
    count = math.ceil((gate.height_m - bottom) / gate.pitch_m)
    bound = 0 if count == 0 else 8 * count + 12 if detail else 12
    if type(max_triangles) is not int or max_triangles < bound:
        raise ValueError("Roller triangle capacity exceeded before expansion")
    section = profile(gate, opened, detail)
    if len(section) == 0:
        return {
            "vertices": np.empty((0, 3)),
            "faces": np.empty((0, 3), dtype=np.uint32),
            "normals": np.empty((0, 3)),
            "uv": np.empty((0, 2)),
            "materials": np.empty(0, dtype=np.uint16),
        }
    triangles = extrusion(section, gate.width_m)
    if len(triangles) > max_triangles:
        raise ValueError("Roller output exceeded its triangle capacity")
    return pack(triangles, section, material)
