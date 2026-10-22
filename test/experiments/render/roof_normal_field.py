"""Ready source-owned normal fields; no form reader, GEOS or geometry cooking."""

import numpy as np


def roof_uv(mesh, field, basis, centre):
    """Map only generated roof corners; original fallback surfaces stay untouched."""
    result = np.full((*mesh["indices"].shape, 2), -1, np.float32)
    keys = {tuple(key): i for i, key in enumerate(field["keys"])}
    binding = mesh["plan_binding"]
    roof = (mesh["native_binding"][:, 0] < 0) & (binding[:, 2] == 1)
    for key in sorted(set(map(tuple, binding[roof, :2]))):
        row = keys[key]
        selected = roof & np.all(binding[:, :2] == key, axis=1)
        parent = (
            mesh["positions"][mesh["indices"][selected]].astype(float) @ basis + centre
        )
        result[selected] = part_uv(field, row, parent)
    return result


def part_uv(field, row, parent):
    frame = field["frames"][row]
    xy = ((np.asarray(parent) - frame[0]) @ np.linalg.inv(frame[1:]))[..., :2]
    low, high = field["xy_bounds"][row]
    fraction = (xy - low) / (high - low)
    x, y, size = field["rects"][row]
    return (fraction * size + [x, y]) / np.array(field["pixels"].shape[1::-1])


def sample_uv(field, uv, basis):
    """Nearest RGB8 normal sample in the display frame; alpha is field eligibility."""
    uv = np.asarray(uv)
    height, width = field["pixels"].shape[:2]
    valid = np.all((uv >= 0) & (uv < 1), axis=-1)
    xy = np.floor(uv * np.array([width, height])).astype(np.int64)
    xy = np.clip(xy, [0, 0], [width - 1, height - 1])
    values = field["pixels"][xy[..., 1], xy[..., 0]]
    valid &= values[..., 3] == 255
    normals = (values[..., :3].astype(float) * (2 / 255) - 1) @ np.asarray(basis).T
    normals /= np.linalg.norm(normals, axis=-1, keepdims=True)
    return normals, valid
