"""Owned OBJ controls with authored UVs/normals and precise material values."""

from pathlib import Path

import numpy as np
from PIL import Image

UV = np.array([[0.1, 0.2], [0.9, 0.2], [0.9, 0.8], [0.1, 0.8]])
VERTICES = np.array(
    [[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [1.0, 1.0, 0.0], [0.0, 1.0, 0.0]]
)
COLOUR = (0.147123456, 0.512345678, 0.812345678, 0.73)


def obj_control(output):
    path = output / "owned-pane.obj"
    path.write_text(
        "mtllib owned-pane.mtl\no pane\ng facade\n"
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "vt 0.1 0.2\nvt 0.9 0.2\nvt 0.9 0.8\nvt 0.1 0.8\nvn 0 0 1\nusemtl facade\n"
        "f -4/-4/-1 -3/-3/-1 -2/-2/-1\nf -4/-4/-1 -2/-2/-1 -1/-1/-1\n"
    )
    (output / "owned-pane.mtl").write_text(
        "newmtl facade\nKd 0.147123456 0.512345678 0.812345678\nd 0.73\n"
    )
    return path


def fixtures(output):
    output = Path(output).resolve()
    if not output.is_relative_to(Path("/tmp")):
        raise ValueError("Generated asset controls require an explicit /tmp directory")
    output.mkdir(parents=True, exist_ok=True)
    return obj_control(output)


def textured_fixture(output):
    path = fixtures(output)
    rgb = np.array(
        [[[32, 64, 96], [96, 64, 32]], [[128, 192, 224], [224, 192, 128]]],
        dtype=np.uint8,
    )
    Image.fromarray(rgb).save(path.with_suffix(".png"))
    with path.with_suffix(".mtl").open("a") as stream:
        stream.write("map_Kd owned-pane.png\n")
    return path
