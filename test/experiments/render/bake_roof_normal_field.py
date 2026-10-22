"""One intrinsic RGB8 normal tile per original planar roof part; no mesh expansion."""

import math

import numpy as np
import shapely
from packed_normals import pack, unpack


def tile(part, size):
    low, high = np.asarray(part.footprint.bounds).reshape(2, 2)
    sample = (np.arange(size) + 0.5) / size
    xx, yy = np.meshgrid(sample, sample)
    xy = np.stack((xx, yy), axis=-1) * (high - low) + low
    points = shapely.points(xy.reshape(-1, 2))
    patches = [(polygon, plane) for polygon, plane in part.patches if polygon.area > 0]
    distances = np.array([shapely.distance(polygon, points) for polygon, _ in patches])
    chosen = np.argmin(distances, axis=0)
    normals = np.array([np.r_[-plane[:2], 1.0] for _, plane in patches])
    frame = np.array([part.surface[k] for k in ["east", "north", "up"]])
    normals = unpack(pack(normals @ frame))
    values = np.empty((size, size, 4), np.uint8)
    values[..., :3] = np.rint(
        (normals[chosen].reshape(size, size, 3) * 0.5 + 0.5) * 255
    ).astype(np.uint8)
    values[..., 3] = 255
    # Extend the nearest original domain beyond the footprint only as a normal
    # value. This does not add coverage or make the tile an opacity product.
    return values, np.stack((low, high))


def bake(parts, size=16):
    if type(size) is not int or not 2 <= size <= 256 or not parts:
        raise ValueError(
            "Nonempty original parts and bounded explicit tile size required"
        )
    keys = [(part.source, part.part) for part in parts]
    if len(set(keys)) != len(keys):
        raise ValueError("Duplicate original source/part field identity")
    grid = math.ceil(math.sqrt(len(parts)))
    stride = size + 2
    pixels = np.zeros((grid * stride, grid * stride, 4), np.uint8)
    bounds, rects, frames = [], [], []
    for i, part in enumerate(parts):
        values, box = tile(part, size)
        x, y = (i % grid) * stride, (i // grid) * stride
        pixels[y : y + stride, x : x + stride] = np.pad(
            values, ((1, 1), (1, 1), (0, 0)), mode="edge"
        )
        bounds.append(box)
        rects.append((x + 1, y + 1, size))
        frames.append([part.surface[k] for k in ["origin", "east", "north", "up"]])
    return {
        "pixels": pixels,
        "keys": np.asarray(keys, np.int32),
        "xy_bounds": np.array(bounds),
        "frames": np.array(frames),
        "rects": np.array(rects, np.int32),
    }
