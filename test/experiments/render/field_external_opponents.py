"""Whole unchanged native originals and explicit analytic terrain/foreign solids."""

import json

import numpy as np
from building_sample_originals import read_original
from building_sample_source import read_source
from triangle_image_reference import intersect_source, prepare_source


def load(directory):
    original, provenance = read_original(directory / "original-geometry")
    metadata, raw, _grid, _view = read_source(
        directory / "source-512", directory / "high-sampling"
    )
    capture = metadata["capture"]
    # Exact subset of native pixel centres, not a recentered 128-square raster.
    y, x = np.mgrid[0 : capture["height_px"] : 4, 0 : capture["width_px"] : 4]
    right, up, forward = (np.asarray(capture[k]) for k in ("right", "up", "forward"))
    offsets = ((x + 0.5) / capture["width_px"] * 2 - 1)[..., None] * capture[
        "x_mag_m"
    ] * right + (1 - (y + 0.5) / capture["height_px"] * 2)[..., None] * capture[
        "y_mag_m"
    ] * up
    origins = (np.asarray(capture["eye_m"]) + offsets).reshape(-1, 3)
    directions = np.broadcast_to(forward, origins.shape)
    original["face_colour"] = np.broadcast_to(
        [1.2, 0.8, 0.45], (len(original["indices"]), 3)
    )
    provenance["capture_subset"] = dict(
        capture, native_stride=4, native_offset=[0.5, 0.5]
    )
    provenance["native_old_scope"] = metadata["scope"]
    return original, provenance, raw, capture, (origins, directions), x.shape


def quad(points, forward, colour):
    points = np.asarray(points)
    ids = np.array([[0, 1, 2], [0, 2, 3]])
    if np.cross(points[1] - points[0], points[2] - points[0]) @ forward > 0:
        ids = ids[:, ::-1]
    return {
        "positions": points,
        "indices": ids,
        "face_colour": np.broadcast_to(colour, (2, 3)),
    }


def join(*sources):
    starts = np.cumsum([0, *(len(s["positions"]) for s in sources[:-1])])
    return {
        "positions": np.concatenate([s["positions"] for s in sources]),
        "indices": np.concatenate(
            [s["indices"] + k for s, k in zip(sources, starts, strict=True)]
        ),
        "face_colour": np.concatenate([s["face_colour"] for s in sources]),
    }


def point_image(source, rays, capture, shape, *, prepared=None):
    prepared = prepare_source(source) if prepared is None else prepared
    depth, faces = intersect_source(source, *rays, prepared=prepared)
    hit = np.isfinite(depth) & (depth >= capture["near_m"]) & (depth < capture["far_m"])
    rgba = np.zeros((len(depth), 4), np.float32)
    normal = prepared["normal"].copy()
    normal /= np.maximum(np.linalg.norm(normal, axis=1), np.finfo(float).tiny)[:, None]
    sun = np.array([0.3, 0.8, 0.5])
    sun /= np.linalg.norm(sun)
    shade = 0.2 + 0.8 * np.maximum(normal @ sun, 0)
    rgba[hit, :3] = source["face_colour"][faces[hit]] * shade[faces[hit], None]
    rgba[hit, 3] = 1
    reverse = np.zeros(len(depth))
    reverse[hit] = (capture["far_m"] - depth[hit]) / (
        capture["far_m"] - capture["near_m"]
    )
    packed = reverse.astype(np.float32)
    # Only Float32 encoding error of these finite CPU observations, not a
    # raster/intersection/geometry error certificate for native original fields.
    error = np.where(hit, np.nextafter(abs(reverse - packed), np.inf), 0)
    return rgba.reshape(*shape, 4), packed.reshape(shape), error.reshape(shape)


def terrain(original, capture):
    lo, hi = original["positions"].min(0), original["positions"].max(0)
    x0, z0 = lo[[0, 2]] - 2 * capture["x_mag_m"]
    x1, z1 = hi[[0, 2]] + 2 * capture["x_mag_m"]
    y = lo[1] - 0.5
    return quad(
        [[x0, y, z0], [x1, y, z0], [x1, y, z1], [x0, y, z1]],
        capture["forward"],
        [0.25, 0.4, 0.15],
    )


def foreign_ring(capture, source_depth):
    values = np.unique(source_depth[source_depth > 0]).astype(float)
    central = np.flatnonzero(
        (values[:-1] > np.quantile(values, 0.4))
        & (values[1:] < np.quantile(values, 0.6))
    )
    # Choose a declared finite control placement between observed depths so
    # this positive interleave is independent of the separate tie refusal.
    at = central[np.argmax(np.diff(values)[central])]
    middle = (values[at] + values[at + 1]) / 2
    centre_depth = capture["far_m"] - middle * (capture["far_m"] - capture["near_m"])
    centre = np.asarray(capture["eye_m"]) + centre_depth * np.asarray(
        capture["forward"]
    )
    right, up = np.asarray(capture["right"]), np.asarray(capture["up"])
    extent = capture["x_mag_m"]
    rectangles = [
        (-0.8, -0.15, -0.8, 0.8),
        (0.15, 0.8, -0.8, 0.8),
        (-0.15, 0.15, -0.8, -0.15),
        (-0.15, 0.15, 0.15, 0.8),
    ]
    return join(
        *(
            quad(
                [
                    centre + extent * (x * right + y * up)
                    for x, y in ((a, c), (b, c), (b, d), (a, d))
                ],
                capture["forward"],
                [0.2, 0.7, 1.2],
            )
            for a, b, c, d in rectangles
        )
    )


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False)
