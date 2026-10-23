"""Source surface-area normal histograms in bricks, not solid/opacity voxels."""

import itertools

import numpy as np
import shapely
from packed_normals import fold, pack, unpack


def normal_table(side):
    x, y = np.meshgrid(np.arange(side), np.arange(side), indexing="ij")
    xy = (np.column_stack((x.ravel(), y.ravel())) + 0.5) * (2 / side) - 1
    z = 1 - np.abs(xy).sum(1)
    xy = np.where(
        z[:, None] < 0, (1 - np.abs(xy[:, ::-1])) * np.where(xy >= 0, 1, -1), xy
    )
    normal = np.column_stack((xy, z))
    normal /= np.linalg.norm(normal, axis=1)[:, None]
    return pack(normal)


def clip_convex(vertices, a, b):
    if not len(vertices):
        return vertices
    result = []
    for first, last in zip(vertices, np.roll(vertices, -1, axis=0), strict=True):
        p, q = first @ a - b, last @ a - b
        if p <= 0:
            result.append(first)
        if (p <= 0) != (q <= 0):
            result.append(first + p / (p - q) * (last - first))
    return np.array(result).reshape(-1, 2)


def clipped_area(patch, low, high):
    left, bottom, right, top = patch.domain.bounds
    polygon = np.array([[left, bottom], [right, bottom], [right, top], [left, top]])
    for axis in range(3):
        polygon = clip_convex(
            polygon, patch.axes[:, axis], high[axis] - patch.origin[axis]
        )
        polygon = clip_convex(
            polygon, -patch.axes[:, axis], patch.origin[axis] - low[axis]
        )
        if len(polygon) < 3:
            return 0.0
    window = shapely.Polygon(polygon)
    if window.area == 0:
        return 0.0
    return float(
        shapely.intersection(patch.domain, window).area
        * np.linalg.norm(np.cross(*patch.axes))
    )


def patch_bricks(patch, pitch):
    uv = shapely.get_coordinates(patch.domain)
    with np.errstate(over="ignore", invalid="ignore"):
        xyz = patch.origin + uv @ patch.axes
        first = np.floor(xyz.min(0) / pitch)
        last = np.maximum(np.ceil(xyz.max(0) / pitch), first + 1)
    limits = np.iinfo(np.int32)
    if (
        not np.isfinite(first).all()
        or not np.isfinite(last).all()
        or np.any(first < limits.min)
        or np.any(last > int(limits.max) + 1)
    ):
        raise ValueError("finite int32 brick coordinates required")
    return first.astype(np.int64), last.astype(np.int64)


def compile_groups(patches, pitch, *, normal_side=16):
    pitch = np.asarray(pitch, float)
    if pitch.shape != (3,) or not np.isfinite(pitch).all() or np.any(pitch <= 0):
        raise ValueError("three finite positive brick dimensions required")
    if not patches or normal_side != 16:
        raise ValueError(
            "known patches and the fixed 16x16 normal-distribution profile required"
        )
    words = normal_table(normal_side)
    normals = unpack(words)
    bounds = [patch_bricks(patch, pitch) for patch in patches]
    clips = sum(
        int(last[0] - first[0]) * int(last[1] - first[1]) * int(last[2] - first[2])
        for first, last in bounds
    )
    if clips > 1048576:
        raise ValueError("bounded total surface-to-brick clipping work required")
    rows = {}
    work = {
        "patches": 0,
        "brick_patch_clips": 0,
        "positive_clips": 0,
        "input_surface_area_m2": 0.0,
    }
    maximum_angle = 0.0
    for patch, (first, last) in zip(patches, bounds, strict=True):
        pair = np.clip(
            np.floor((fold(patch.normal[None])[0] + 1) * normal_side / 2),
            0,
            normal_side - 1,
        ).astype(int)
        code = int(pair[0] * normal_side + pair[1])
        maximum_angle = max(
            maximum_angle,
            float(np.degrees(np.arccos(np.clip(normals[code] @ patch.normal, -1, 1)))),
        )
        work["input_surface_area_m2"] += float(
            patch.domain.area * np.linalg.norm(np.cross(*patch.axes))
        )
        work["patches"] += 1
        for cell in itertools.product(
            *(range(int(a), int(b)) for a, b in zip(first, last, strict=True))
        ):
            low = np.asarray(cell) * pitch
            area = clipped_area(patch, low, low + pitch)
            work["brick_patch_clips"] += 1
            if area > 0:
                key = (*cell, code)
                rows[key] = rows.get(key, 0.0) + area
                work["positive_clips"] += 1
    ordered = sorted(rows)
    if not ordered:
        raise ValueError("no represented surface measure")
    indices = np.array(ordered, np.int32)
    cells, counts = np.unique(indices[:, :3], axis=0, return_counts=True)
    arrays = {
        "cells": cells,
        "offsets": np.r_[0, np.cumsum(counts)].astype(np.uint32),
        "normal_ids": indices[:, 3].astype(np.uint8),
        "area_m2": np.array([rows[key] for key in ordered]),
        "normal_words": words,
        "pitch_m": pitch,
    }
    work.update(
        bricks=len(cells),
        distribution_records=len(ordered),
        stored_surface_area_m2=float(arrays["area_m2"].sum()),
        maximum_normal_quantization_angle_deg=maximum_angle,
    )
    return arrays, work
