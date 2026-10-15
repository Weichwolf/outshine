"""Own leaf-local midrib/side veins, linear pigment and physical ridge height."""

import json

import numpy as np
from PIL import Image


def write_maps(output):
    u, v = np.meshgrid((np.arange(256) + 0.5) / 256, (np.arange(256) + 0.5) / 256)
    x = abs(u - 0.5)
    midrib = np.exp(-((x / 0.014) ** 2))
    side = np.maximum(0, np.cos(2 * np.pi * (12 * v - 3 * x))) ** 24 * np.exp(-2 * x)
    colour = (
        np.array((0.04, 0.16, 0.018))[None, None]
        * (1 + 0.04 * np.sin(19 * u + 7 * v))[..., None]
    )
    colour += (midrib * 0.5 + side * 0.13)[..., None] * (0.04, 0.07, 0.008)
    height = 0.000045 * midrib + 0.000012 * side
    Image.fromarray(np.rint(np.clip(colour, 0, 1) * 255).astype(np.uint8)).save(
        output / "fern-colour.png"
    )
    Image.fromarray(np.rint(height / 0.00006 * 65535).astype(np.uint16)).save(
        output / "fern-height.png"
    )
    (output / "fern-material.json").write_text(
        json.dumps(
            {
                "format": "fern-leaf-study-1",
                "colour": "fern-colour.png",
                "height": "fern-height.png",
                "height_scale_m": 0.00006,
                "colour_space": "linear data",
                "scope": "Own authored pigment/ridges; qualitative thin transmission, not calibrated species BSDF or native mip filtering.",
            },
            indent=2,
        )
        + "\n"
    )
