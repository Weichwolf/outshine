"""Explicit paired old-boolean/new-source-index light inputs for native controls."""

import numpy as np

CASES = (
    ("first", 2, 0, 0, True),
    ("last", 2, 1, 0, True),
    ("middle16", 16, 7, 0, True),
    ("last16", 16, 15, 0, True),
    ("single", 1, 0, 0, True),
    ("none", 2, 0, 0, False),
    ("point", 2, 1, 1, True),
    ("spot", 2, 1, 2, True),
)


def fixture(offsets, material, count, source, kind, atlas):
    m = np.zeros(96, "<f4")
    for name, value in (
        ("factor", 1),
        ("roughness", 1),
        ("basex", 0.5),
        ("basey", 0.5),
        ("basez", 0.5),
        ("basew", 1),
        ("scattering", 1 if material == "thin" else 0),
    ):
        m[offsets[name]] = value
    lights = np.zeros(336, "<f4")
    lights[:4] = [count, 0, source + 1 if atlas else 0, 1]
    lights[20:24] = [0, 10, 0, 1]
    lights[24:28] = [0, 1, 0, 0]
    s = np.float32(np.sqrt(0.5))
    lights[28:32] = [s, s, 0, 0]
    lights[32:36], lights[36:40], lights[40:44] = (
        [1, 1, 0, 0],
        [1, 0, 0, 0],
        [0, 0, 1, 0],
    )
    for index in range(count):
        at = 80 + 16 * index
        if index == source:
            lights[at : at + 4] = [1, 0, 0, 0]
            lights[at + 8 : at + 12] = [-s, -s, 0, 0]
        else:
            lights[at : at + 4] = [0, 0, 1 / (count - 1), kind]
            lights[at + 4 : at + 8] = [0, 2, 0, 0]
            lights[at + 8 : at + 12] = [0.6, -0.8, 0, 0] if kind == 0 else [0, -1, 0, 0]
            lights[at + 12 : at + 16] = [0.8, 1 / (0.95 - 0.8), 0, 0]
    before = lights.copy()
    before[2] = int(atlas)
    return m.tobytes() + before.tobytes(), m.tobytes() + lights.tobytes()
