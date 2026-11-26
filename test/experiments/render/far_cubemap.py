"""Reference image-space geometry for a single-layer colour/radial-depth cubemap."""

import numpy as np

# Cube slice order +X, -X, +Y, -Y, +Z, -Z. Pixel coordinates grow right and down.
FORWARD = np.array([(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)])
RIGHT = np.array([(0, 0, -1), (0, 0, 1), (1, 0, 0), (1, 0, 0), (1, 0, 0), (-1, 0, 0)])
DOWN = np.array([(0, -1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1), (0, -1, 0), (0, -1, 0)])


def unit(vectors):
    return vectors / np.linalg.norm(vectors, axis=-1, keepdims=True)


def directions(side):
    coords = (np.arange(side) + .5) * (2 / side) - 1
    return unit(FORWARD[:, None, None] + coords[None, None, :, None] * RIGHT[:, None, None] +
                coords[None, :, None, None] * DOWN[:, None, None])


def addresses(directions, side):
    axis = np.argmax(np.abs(directions), axis=-1)
    major = np.take_along_axis(directions, axis[..., None], axis=-1)[..., 0]
    face = axis * 2 + (major < 0)
    uv = np.stack([np.sum(directions * basis[face], axis=-1) / abs(major)
                   for basis in (RIGHT, DOWN)], axis=-1)
    xy = np.clip(((uv + 1) * (.5 * side)).astype(np.int64), 0, side - 1)
    return face, xy[..., 1], xy[..., 0]


def from_device_depth(device_depth, near, far):
    """Convert reversed face-camera z to radial metres and mirror the native camera rows."""
    if device_depth.ndim != 2 or device_depth.shape[0] != device_depth.shape[1]:
        raise ValueError("square face depth required")
    if not 0 < near < far or not np.isfinite(device_depth).all():
        raise ValueError("finite depth and matching perspective planes required")
    if np.any((device_depth < 0) | (device_depth > 1)):
        raise ValueError("device depth outside [0, 1]")
    side = len(device_depth)
    ndc = (np.arange(side) + .5) * (2 / side) - 1
    radial_scale = np.sqrt(1 + ndc[:, None] ** 2 + ndc[None, :] ** 2)
    reverse_bias = near / (far - near)
    radial = radial_scale * (far * reverse_bias / (device_depth + reverse_bias))
    return np.where(device_depth > 0, radial, 0)[:, ::-1].astype(np.float32)


def reproject(depth, rays, eye_delta, focal_px):
    """Inverse warp from a nearby eye. Zero depth denotes sky or unresolved coverage."""
    side = depth.shape[1]
    eye = np.asarray(eye_delta, np.float64)
    distance2 = float(eye @ eye)
    eye_dot = rays @ eye
    across2 = np.maximum(0, distance2 - eye_dot**2)
    index = addresses(rays, side)
    if distance2 == 0:
        return index, depth[index].astype(np.float64), np.ones(rays.shape[:-1], bool)
    sky = depth[index] == 0
    stable = np.zeros_like(sky)
    distance = np.zeros(sky.shape)
    for _ in range(3):
        radial = depth[index].astype(np.float64)
        discriminant = radial**2 - across2
        distance = np.sqrt(np.maximum(discriminant, 0)) - eye_dot
        lookup = unit(eye + np.maximum(distance, 1)[..., None] * rays)
        new_index = addresses(lookup, side)
        new_radius = depth[new_index].astype(np.float64)
        finite = (radial > 0) & (new_radius > 0) & (discriminant > 0) & (distance > 0)
        inverse_change = abs(new_radius - radial) / np.maximum(new_radius * radial, 1)
        stable = finite & (inverse_change * np.sqrt(across2) * focal_px < .5)
        index = new_index
    rad = depth[index].astype(np.float64)
    distance = np.sqrt(np.maximum(rad**2 - across2, 0)) - eye_dot
    original = addresses(rays, side)
    return (tuple(np.where(stable, revised, direct) for revised, direct in zip(index, original)),
            np.where(stable, distance, 0), stable | sky)
