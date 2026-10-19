"""Shared diagnostic native colour-factor/normal shading, not native PBR/UVs."""

import numpy as np
from packed_normals import unpack


def surface_fields(source, faces):
    valid = faces >= 0
    normal = np.zeros((len(faces), 3))
    colour = np.zeros((len(faces), 3))
    code = np.zeros(len(faces), np.int16)
    normal[valid] = unpack(source["normal"][faces[valid]])
    colour[valid] = source["palette"][source["colour_index"][faces[valid]], :3]
    code[valid] = source["code"][faces[valid]]
    return {"normal": normal, "colour": colour, "code": code}


def relight(fields, sun, depth):
    sun = np.array(sun, float)
    sun /= np.linalg.norm(sun)
    light = 0.2 + 0.8 * np.maximum(fields["normal"] @ sun, 0)
    rgb = np.clip(fields["colour"] * light[:, None], 0, 1)
    return np.where(np.isfinite(depth)[:, None], rgb, 0)
