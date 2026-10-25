"""One shared oblique construction camera; geometric normal shading, no PBR."""

import numpy as np
from current_visibility import raster
from PIL import Image


def construction(original, candidate, output):
    forward = np.array([-0.45, 0.55, -0.7])
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, [0, 0, 1])
    right /= np.linalg.norm(right)
    basis = np.array([right, np.cross(right, forward), forward])
    all_points = np.vstack((original["positions"], candidate["positions"])) @ basis.T
    low, high = all_points.min(0), all_points.max(0)
    centre = (low + high) / 2
    scale = max((high[0] - low[0]) / 1280, (high[1] - low[1]) / 720) / 0.94
    light = np.array([0.45, -0.5, 0.7])
    light /= np.linalg.norm(light)
    images = []
    for name, source in (("original", original), ("mass", candidate)):
        p = source["positions"] @ basis.T
        z = p[:, 2] - low[2] + 1
        projected = np.column_stack(
            (
                640 + (p[:, 0] - centre[0]) / scale,
                360 - (p[:, 1] - centre[1]) / scale,
                0.1 / z,
            )
        )
        _, faces = raster(projected, z, source["indices"], 1280, 720, with_faces=True)
        triangles = source["positions"].astype(float)[source["indices"]]
        normals = np.cross(
            triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        )
        lengths = np.linalg.norm(normals, axis=1)
        normals /= np.where(lengths > 0, lengths, 1)[:, None]
        intensity = 0.18 + 0.7 * np.maximum(normals @ light, 0)
        rgb = np.full((720, 1280, 3), 24, np.uint8)
        hit = faces >= 0
        rgb[hit] = np.rint(255 * intensity[faces[hit]])[:, None]
        images.append(Image.fromarray(rgb))
        images[-1].save(output / (name + "-construction.png"))
    contact = Image.new("RGB", (2560, 720))
    for i, picture in enumerate(images):
        contact.paste(picture, (1280 * i, 0))
    contact.save(output / "construction.png")
