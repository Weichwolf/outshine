"""Closed own ellipsoidal bud; attachment at origin, tip along local minus Z."""

import numpy as np


def bud_mesh(length, radius, segments=8, sides=16):
    if (
        not np.isfinite((length, radius)).all()
        or not 1e-6 <= min(length, radius) <= max(length, radius) <= 1
        or type(segments) is not int
        or not 4 <= segments <= 64
        or type(sides) is not int
        or not 6 <= sides <= 128
    ):
        raise ValueError("bounded positive bud dimensions and subdivisions required")
    phi, theta = np.meshgrid(
        np.arange(1, segments) * np.pi / segments,
        np.arange(sides) * 2 * np.pi / sides,
        indexing="ij",
    )
    ring = np.stack(
        (
            radius * np.sin(phi) * np.cos(theta),
            radius * np.sin(phi) * np.sin(theta),
            length * (np.cos(phi) - 1) / 2,
        ),
        axis=-1,
    ).reshape(-1, 3)
    vertices = np.vstack((np.zeros(3), ring, (0, 0, -length)))
    i = 1 + np.arange(segments - 2)[:, None] * sides + np.arange(sides)
    j = 1 + ((i - 1) // sides) * sides + i % sides
    top = np.c_[
        np.zeros(sides, int), 1 + np.arange(sides), 1 + (np.arange(sides) + 1) % sides
    ]
    bottom = np.c_[
        np.full(sides, len(vertices) - 1),
        1 + (segments - 2) * sides + (np.arange(sides) + 1) % sides,
        1 + (segments - 2) * sides + np.arange(sides),
    ]
    faces = np.vstack(
        (
            top,
            np.c_[i.ravel(), (i + sides).ravel(), j.ravel()],
            np.c_[j.ravel(), (i + sides).ravel(), (j + sides).ravel()],
            bottom,
        )
    ).astype(np.int32)
    normals = (vertices + (0, 0, length / 2)) / (
        radius**2,
        radius**2,
        (length / 2) ** 2,
    )
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    uv = np.c_[
        np.arccos(np.clip(1 + 2 * vertices[:, 2] / length, -1, 1)) / np.pi,
        np.mod(np.arctan2(vertices[:, 1], vertices[:, 0]), 2 * np.pi) / (2 * np.pi),
    ][faces.ravel()].reshape(-1, 3, 2)
    seam = np.ptp(uv[:, :, 1], axis=1) > 0.5
    uv[seam, :, 1] = np.where(uv[seam, :, 1] < 0.5, uv[seam, :, 1] + 1, uv[seam, :, 1])
    # Longitude is undefined at each pole: use the midpoint of the adjacent ring pair.
    uv[:sides, 0, 1] = uv[:sides, 1:, 1].mean(axis=1)
    uv[-sides:, 0, 1] = uv[-sides:, 1:, 1].mean(axis=1)
    return {
        "vertices": vertices,
        "faces": faces,
        "normals": normals[faces.ravel()],
        "corner_uv": uv.reshape(-1, 2),
    }
