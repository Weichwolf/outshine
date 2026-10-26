"""Camera-independent window recesses from an existing facade repeat, in local U/V/outward metres."""

import math
from dataclasses import dataclass

import numpy as np

from .facades import FacadeRepeat


@dataclass(frozen=True, slots=True)
class RecessPlan:
    repeat: FacadeRepeat
    depth_m: float = 0.18
    front_step_m: float = 0.04
    frame_m: tuple[float, float] = (0.06, 0.06)

    def __post_init__(self):
        if not isinstance(self.repeat, FacadeRepeat):
            raise TypeError("Recesses require an existing FacadeRepeat")
        if not isinstance(self.frame_m, tuple) or len(self.frame_m) != 2:
            raise ValueError("Recess frame widths require one immutable pair")
        values = (self.depth_m, self.front_step_m, *self.frame_m)
        if any(isinstance(x, (bool, np.bool_)) or not math.isfinite(x) for x in values):
            raise ValueError("Finite metric recess/frame dimensions required")
        opening = np.array(self.repeat.cell_size_m) * [
            self.repeat.opening_width,
            self.repeat.opening_height,
        ]
        if (
            not 0 < self.front_step_m < self.depth_m
            or any(
                not 0 < x < 1
                for x in (self.repeat.opening_width, self.repeat.opening_height)
            )
            or np.any(np.asarray(self.frame_m) <= 0)
            or np.any(2 * np.asarray(self.frame_m) >= opening)
        ):
            raise ValueError(
                "Recess needs opaque margins, a positive frame and an interior glass aperture"
            )

    def count(self, bounds=None):
        x0, y0, x1, y1 = self.bounds(bounds)
        return (x1 - x0) * (y1 - y0)

    def bounds(self, bounds=None):
        values = (
            (0, 0, self.repeat.columns, self.repeat.rows) if bounds is None else bounds
        )
        if len(values) != 4 or any(type(x) is not int for x in values):
            raise ValueError("Recess cell bounds require four integers")
        x0, y0, x1, y1 = values
        if (
            not 0 <= x0 <= x1 <= self.repeat.columns
            or not 0 <= y0 <= y1 <= self.repeat.rows
        ):
            raise ValueError("Recess cell bounds are outside the original repeat")
        return tuple(values)


def cell_template(plan):
    size = np.asarray(plan.repeat.cell_size_m)
    lo = (
        size
        * (1 - np.array([plan.repeat.opening_width, plan.repeat.opening_height]))
        / 2
    )
    hi = size - lo
    x, y = [0, lo[0], hi[0], size[0]], [0, lo[1], hi[1], size[1]]
    quads, roles = [], []

    def rectangle(a, b, depth, role):
        quads.append(
            [
                [a[0], a[1], depth],
                [b[0], a[1], depth],
                [b[0], b[1], depth],
                [a[0], b[1], depth],
            ]
        )
        roles.append(role)

    for j in range(3):
        for i in range(3):
            if i != 1 or j != 1:
                rectangle((x[i], y[j]), (x[i + 1], y[j + 1]), 0, 0)
    outer = np.array([[lo[0], lo[1]], [hi[0], lo[1]], [hi[0], hi[1]], [lo[0], hi[1]]])
    frame = np.asarray(plan.frame_m)
    inner = outer + [
        [frame[0], frame[1]],
        [-frame[0], frame[1]],
        [-frame[0], -frame[1]],
        [frame[0], -frame[1]],
    ]
    for loop, low, high, role in (
        (outer, 0, -plan.front_step_m, 0),
        (inner, -plan.front_step_m, -plan.depth_m, 2),
    ):
        for a, b in zip(loop, np.roll(loop, -1, axis=0), strict=True):
            quads.append([[*a, low], [*b, low], [*b, high], [*a, high]])
            roles.append(role)
    for i in range(4):
        j = (i + 1) % 4
        points = np.array([outer[i], outer[j], inner[j], inner[i]])
        quads.append(np.c_[points, np.full(4, -plan.front_step_m)])
        roles.append(2)
    rectangle(inner[0], inner[2], -plan.depth_m, 1)
    quads = np.asarray(quads, dtype=np.float64)
    normals = np.cross(quads[:, 1] - quads[:, 0], quads[:, 2] - quads[:, 0])
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    faces = (
        np.arange(len(quads))[:, None, None] * 4 + np.array([[0, 1, 2], [0, 2, 3]])
    ).reshape(-1, 3)
    return (
        quads.reshape(-1, 3),
        faces.astype(np.uint32),
        np.repeat(normals, 4, axis=0),
        np.repeat(roles, 2).astype(np.uint8),
    )


def recess_surface(plan, bounds=None, *, max_cells=50000, method="batch"):
    """Caller-owned arrays; roles 0 wall, 1 glass, 2 frame. UVs retain original repeat cells.

    Bounds are half-open global cell indices, selected before this expansion.
    This surface has an open perimeter; the caller retains the source building shell.
    It provides no door, room, collision, material table or visibility decision.
    """
    if not isinstance(plan, RecessPlan) or type(max_cells) is not int or max_cells < 0:
        raise ValueError("RecessPlan and nonnegative integer cell capacity required")
    bounds = plan.bounds(bounds)
    count = plan.count(bounds)
    if count > max_cells or count * 84 > np.iinfo(np.uint32).max:
        raise ValueError("Recess expansion exceeds the declared cell/index capacity")
    if method not in ("batch", "scalar"):
        raise ValueError("Declared batch or scalar construction required")
    vertices, faces, normals, roles = cell_template(plan)
    x0, y0, x1, _y1 = bounds
    indices = np.arange(count)
    width = max(1, x1 - x0)
    cells = np.c_[x0 + indices % width, y0 + indices // width]
    offsets = np.c_[cells * plan.repeat.cell_size_m, np.zeros(count)]
    if method == "batch":
        points = (vertices[None, :, :] + offsets[:, None, :]).reshape(-1, 3)
    else:
        points = (
            np.concatenate([vertices + offset for offset in offsets])
            if count
            else np.empty((0, 3))
        )
    return {
        "vertices": points,
        "faces": (
            faces[None, :, :]
            + np.arange(count, dtype=np.uint32)[:, None, None] * len(vertices)
        ).reshape(-1, 3),
        "normals": np.tile(normals, (count, 1)),
        "uv": points[:, :2] / plan.repeat.cell_size_m,
        "roles": np.tile(roles, count),
        "cells": np.repeat(cells.astype(np.uint32), len(faces), axis=0),
    }
