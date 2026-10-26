"""Orthographic construction wires, explicitly not a rendered-image qualification."""

import numpy as np
from PIL import Image, ImageDraw


def plot_meshes(control, candidate, original_bytes, candidate_bytes, path):
    right = np.array([1, 1, 0]) / np.sqrt(2)
    back = np.array([1, -1, 1]) / np.sqrt(3)
    up = np.cross(back, right)
    basis = np.stack((right, -up))
    projected = [
        mesh["positions"].astype(float) @ basis.T for mesh in (control, candidate)
    ]
    low = np.minimum(*(p.min(0) for p in projected))
    high = np.maximum(*(p.max(0) for p in projected))
    scale = 490 / (high - low).max()
    image = Image.new("RGB", (1060, 570), (27, 27, 27))
    draw = ImageDraw.Draw(image)
    for panel, (mesh, points, label, amount) in enumerate(
        zip(
            (control, candidate),
            projected,
            ("Unshifted source patches", "3m / 0.5 degree plane unions"),
            (original_bytes, candidate_bytes),
            strict=True,
        )
    ):
        draw.text((panel * 530 + 12, 10), label, fill="white")
        xy = (points - low) * scale + [panel * 530 + 12, 40]
        for triangle in mesh["indices"]:
            vertices = xy[triangle]
            draw.line(
                np.concatenate((vertices, vertices[:1])).ravel().tolist(),
                fill=(155, 155, 155),
                width=1,
            )
        draw.text(
            (panel * 530 + 12, 535),
            f"Position/index bit control: {amount} B",
            fill="white",
        )
    draw.text(
        (12, 554),
        "Same orthographic scale; construction wires only, no visibility, materials, coverage, shadow or PBR admission.",
        fill=(210, 210, 210),
    )
    image.save(path)
