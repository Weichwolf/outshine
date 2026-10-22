"""Diagnostic meshes share the ready recipe; opaque boxes are an independent control."""

import numpy as np
from urban_patches import descriptors


def height_atlas(product, chunks):
    columns = max(1, int(np.ceil(np.sqrt(len(chunks)))))
    rows = max(1, int(np.ceil(len(chunks) / columns)))
    image = np.zeros((rows * product.side, columns * product.side, 4), np.float32)
    image[:, :, 3] = 1
    for index, chunk in enumerate(chunks):
        x, y = index % columns * product.side, index // columns * product.side
        height = product.heights[product.tiers[chunk]]
        image[y : y + product.side, x : x + product.side, :3] = height[:, :, None]
    return image, columns, rows


def quad(axis, sign, coordinate, low, high):
    other = [(axis + 1) % 3, (axis + 2) % 3]
    points = np.zeros((4, 3))
    points[:, axis] = coordinate
    points[:, other[0]] = [low[other[0]], high[other[0]], high[other[0]], low[other[0]]]
    points[:, other[1]] = [low[other[1]], low[other[1]], high[other[1]], high[other[1]]]
    return points if sign > 0 else points[[0, 3, 2, 1]]


def build(product, chunks, mode):
    if mode not in {"boxes", "masks"}:
        raise ValueError("explicit box or masked-plane diagnostic mode required")
    vertices, kinds, levels, materials, grid_uv = [], [], [], [], []
    for material_index, chunk in enumerate(chunks):
        origin, pitch = product.origin(chunk), product.pitch
        low = np.array([*origin, 0.0])
        high = np.array(
            [
                *(origin + product.side * pitch),
                product.heights[product.tiers[chunk]].max(),
            ]
        )
        if mode == "masks":
            faces = [
                (quad(a, s, c, low, high), a + 1 if a < 2 else 0 if f == 4 else 3, c)
                for a, s, c, f, _ in descriptors(product, chunk)
            ]
        else:
            faces = []
            half = product.frame[4] * pitch / 2
            for y, x in np.argwhere(product.tiers[chunk] > 0):
                centre = origin + (np.array([x, y]) + 0.5) * pitch
                bottom = np.array([*(centre - half), 0.0])
                top = np.array(
                    [*(centre + half), product.heights[product.tiers[chunk, y, x]]]
                )
                for axis in range(3):
                    for sign in [-1, 1]:
                        faces.append(
                            (
                                quad(
                                    axis,
                                    sign,
                                    bottom[axis] if sign < 0 else top[axis],
                                    bottom,
                                    top,
                                ),
                                -1,
                                0,
                            )
                        )
        for points, kind, height in faces:
            vertices.append(points)
            grid_uv.append((points[:, :2] - origin) / (product.side * pitch))
            kinds.extend([kind] * 4)
            levels.extend([height] * 4)
            materials.append(material_index)
    points = np.concatenate(vertices) if vertices else np.empty((0, 3))
    return {
        "positions": points,
        "quads": np.arange(len(points), dtype=np.uint32).reshape(-1, 4),
        "grid_uv": np.concatenate(grid_uv) if grid_uv else np.empty((0, 2)),
        "plane_kind": np.array(kinds, np.float32),
        "plane_height": np.array(levels, np.float32),
        "material": np.array(materials, np.int32),
    }
