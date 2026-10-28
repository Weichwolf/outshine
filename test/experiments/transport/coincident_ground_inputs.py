"""Unchanged actual source component and additional retained southern DEM window."""

import hashlib
import json
from pathlib import Path

import numpy as np
from ground_cyclic_inputs import prepare as prepare_census

FIXTURE = Path(__file__).with_name("fixtures") / "coincident-ground-vienna-dem.json"
CASE = "1980174443833108642"


def prepare():
    index, cases, _old_terrain, budget, metadata = prepare_census(
        minimum_cycles=3, maximum_cycles=8, maximum_spans=32
    )
    source_case = next(case for case in cases if case["key"] == CASE)
    receipt = json.loads(FIXTURE.read_text())
    path = FIXTURE.with_name(receipt["window_file"])
    raw = path.read_bytes()
    if (
        len(raw) != receipt["window_bytes"]
        or hashlib.sha256(raw).hexdigest() != receipt["window_sha256"]
    ):
        raise ValueError("cover_fixed_dem_window_changed")
    with np.load(path, allow_pickle=False) as stored:
        height, origin, shape = (
            stored[key].copy()
            for key in ("heights_m", "origin_pixel_xy", "raster_shape")
        )
    if (
        height.shape != tuple(receipt["window_shape"])
        or origin.tolist() != receipt["origin_pixel_xy"]
        or shape.tolist() != receipt["raster_shape"]
        or not np.isfinite(height).all()
    ):
        raise ValueError("cover_fixed_dem_layout_changed")
    height.setflags(write=False)
    zoom, tile_x, tile_y = receipt["tile"]

    def terrain(geographic):
        lon, lat = np.asarray(geographic).T
        pixel = (
            np.c_[
                (lon + 180) / 360 * 2**zoom - tile_x,
                (1 - np.arcsinh(np.tan(np.deg2rad(lat))) / np.pi) / 2 * 2**zoom
                - tile_y,
            ]
            * (shape[::-1] - 1)
            - origin
        )
        base = np.floor(pixel).astype(int)
        if np.any(base < 0) or np.any(base >= np.array(height.shape[::-1]) - 1):
            raise ValueError("cover_missing_actual_dem_stencil")
        u, v = (pixel - base).T
        x, y = base.T
        return (1 - v) * ((1 - u) * height[y, x] + u * height[y, x + 1]) + v * (
            (1 - u) * height[y + 1, x] + u * height[y + 1, x + 1]
        )

    return index, source_case, terrain, budget, metadata | {"cover_terrain": receipt}
