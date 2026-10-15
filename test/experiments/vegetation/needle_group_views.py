"""Bounded unlit views keep coverage, raw normals and depth separate from lighting."""

import numpy as np
from leaf_sheet import average_blocks
from outshine_ref.vegetation.shoot_plan import normal_frame
from raster_batch import rasterize


def projection(points, direction):
    direction = np.asarray(direction, float)
    if (
        direction.shape != (3,)
        or not np.isfinite(direction).all()
        or np.linalg.norm(direction) == 0
    ):
        raise ValueError("finite nonzero viewing direction required")
    direction = direction / np.linalg.norm(direction)
    first = np.argmin(abs(direction))
    x = np.eye(3)[first] - direction[first] * direction
    frame = normal_frame(x, direction)
    p = points @ frame
    lower, upper = p[:, :2].min(axis=0), p[:, :2].max(axis=0)
    centre = (lower + upper) / 2
    half = (upper - lower) * 0.55
    return frame, centre - half, centre + half


def render_view(source, direction, size=64, bounds=None, samples=2):
    if type(size) is not int or size not in (16, 32, 64, 128):
        raise ValueError("bounded power-of-two shoot view size required")
    if type(samples) is not int or samples not in (1, 2, 4, 8):
        raise ValueError("one to eight bounded sample rows per texel required")
    frame, lower, upper = projection(source["vertices"], direction)
    if bounds is not None:
        lower, upper = bounds
    p = source["vertices"] @ frame
    attributes = np.concatenate(
        (
            source["normals"] @ frame,
            source["colour"],
            source["roughness"],
            p[source["faces"], 2, None],
        ),
        axis=-1,
    )
    covered, data = rasterize(
        p, source["faces"], attributes, lower, upper, size * samples
    )
    n = data[..., :3]
    n /= np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-20)
    alpha = covered.astype(float)[..., None]
    rgba = average_blocks(
        np.concatenate((data[..., 3:6] * alpha, alpha), axis=-1), samples
    )
    normal = average_blocks(n * alpha, samples)
    depth = average_blocks(data[..., 7:8] * alpha, samples)
    roughness = average_blocks(data[..., 6:7] ** 4 * alpha, samples)
    raw = np.concatenate((n, data[..., 3:8], alpha.astype(data.dtype)), axis=-1)
    raw = (
        raw.reshape(size, samples, size, samples, 9)
        .transpose(0, 2, 1, 3, 4)
        .reshape(size, size, samples * samples, 9)
    )
    return {
        "frame": frame,
        "lower": lower,
        "upper": upper,
        "rgba": rgba,
        "normal": normal,
        "depth": depth,
        "roughness_fourth": roughness,
        "raw_samples": raw,
        "pixel_area_m2": float(np.prod(upper - lower) / size**2),
    }


def load_views(path, expected_identity):
    """Decode the complete cached views without any organ placement or meshing."""
    with np.load(path, allow_pickle=False) as stored:
        if str(stored["identity"]) != expected_identity:
            raise ValueError("cached view plan identity mismatch")
        views = []
        for index in range(64):
            prefix = f"view_{index}_"
            view = {
                key.removeprefix(prefix): stored[key]
                for key in stored.files
                if key.startswith(prefix)
            }
            for value in view.values():
                value.setflags(write=False)
            size = view["rgba"].shape[0]
            view["pixel_area_m2"] = float(
                np.prod(view["upper"] - view["lower"]) / size**2
            )
            views.append(view)
    return views
