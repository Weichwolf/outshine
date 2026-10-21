"""Explicit opaque layered geometry with a narrow opening; not a native world input."""

import numpy as np


def scene():
    vertices, faces, owners = [], [], []

    def quad(x0, x1, y0, y1, z, owner):
        offset = len(vertices)
        vertices.extend([[x0, y0, z], [x1, y0, z], [x1, y1, z], [x0, y1, z]])
        faces.extend(
            [[offset, offset + 1, offset + 2], [offset, offset + 2, offset + 3]]
        )
        owners.extend([owner, owner])

    quad(-9, -0.03, -5, 5, 8, 0)
    quad(0.03, 9, -5, 5, 8, 1)
    owner = 2
    for y0 in np.linspace(-6, 4.8, 10):
        for x0 in np.linspace(-12, 10.8, 20):
            quad(x0, x0 + 1.15, y0, y0 + 1.15, 24, owner)
            owner += 1
    document = {
        "sources": [{"index": i, "id": f"analytic-opaque-{i}"} for i in range(owner)]
    }
    view = np.zeros(3), np.eye(3), 691.5535657096198, 1280, 720
    return (
        np.array(vertices),
        np.array(faces, dtype=np.int64),
        np.array(owners),
        document,
        view,
    )
