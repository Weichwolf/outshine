"""Own linear-colour and physical ridge-height maps; no photographic inputs."""

import json

import numpy as np
from PIL import Image


def write_maps(output):
    x, y = np.meshgrid((np.arange(256) + 0.5) / 256, (np.arange(256) + 0.5) / 256)
    central = np.exp(-(((x - 0.5) / 0.012) ** 2))
    parallel = np.sin(14 * np.pi * x) ** 16
    minor = np.sin(42 * np.pi * x + 0.08 * np.sin(5 * y)) ** 12
    colour = (
        np.array((0.035, 0.14, 0.022))[None, None]
        * (1 + 0.05 * np.sin(7.3 * x + 4.1 * y))[..., None]
    )
    colour += (central * 0.6 + parallel * 0.15)[..., None] * (0.035, 0.055, 0.005)
    height_m = 0.00008 * central + 0.00002 * parallel + 0.000002 * minor
    Image.fromarray(np.rint(np.clip(colour, 0, 1) * 255).astype(np.uint8)).save(
        output / "parallel-leaf-colour.png"
    )
    Image.fromarray(
        np.rint(np.clip(height_m / 0.0001, 0, 1) * 65535).astype(np.uint16)
    ).save(output / "parallel-leaf-height.png")
    (output / "parallel-leaf-material.json").write_text(
        json.dumps(
            {
                "format": "parallel-leaf-preview-1",
                "size": [256, 256],
                "colour_space": "linear data",
                "height_scale_m": 0.0001,
                "scope": "Authored converging UV veins; host image filtering, no measured species BSDF or native normal-variance admission.",
            },
            indent=2,
        )
        + "\n"
    )
