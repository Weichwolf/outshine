"""Blender-only Float32 UDIM staging for independent prototype-view mip chains."""

import hashlib
import json
from pathlib import Path

import bpy
import numpy as np


def write_tiles(values, directory):
    directory = Path(directory)
    if not directory.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private field staging must be under/tmp")
    values = np.asarray(values, np.float32)
    if values.shape != (64, 32, 32, 3) or not np.isfinite(values).all():
        raise ValueError("same bounded finite64x32x32vector field required")
    directory.mkdir(parents=True, exist_ok=False)
    stage = bpy.data.images.new(
        "one reusable field staging buffer",
        width=32,
        height=32,
        alpha=True,
        float_buffer=True,
    )
    stage.colorspace_settings.name = "Non-Color"
    stage.alpha_mode = "CHANNEL_PACKED"
    stage.use_half_precision = False
    scene = bpy.context.scene
    scene.render.image_settings.file_format = "OPEN_EXR"
    scene.render.image_settings.color_depth = "32"
    pixels = np.ones((32, 32, 4), np.float32)
    try:
        for index, value in enumerate(values):
            pixels[..., :3] = value
            stage.pixels.foreach_set(pixels.ravel())
            stage.save_render(str(directory / f"field.{1001 + index}.exr"), scene=scene)
    finally:
        bpy.data.images.remove(stage)
    receipt = {
        "version": "isolated-normal-view-tiles-1",
        "source_sha256": hashlib.sha256(values.tobytes()).hexdigest(),
        "size": [32, 32],
        "views": 64,
        "format": "Float32 RGBA, non-colour",
        "tile_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(directory.glob("*.exr"))
        },
    }
    (directory / "field.json").write_text(json.dumps(receipt, indent=2) + "\n")


def load_tiles(values, directory):
    directory = Path(directory)
    values = np.asarray(values, np.float32)
    receipt = json.loads((directory / "field.json").read_text())
    if (
        values.shape != (64, 32, 32, 3)
        or not np.isfinite(values).all()
        or receipt["version"] != "isolated-normal-view-tiles-1"
        or receipt["source_sha256"] != hashlib.sha256(values.tobytes()).hexdigest()
        or set(receipt["tile_sha256"]) != {f"field.{1001 + i}.exr" for i in range(64)}
    ):
        raise ValueError("same complete immutable prepared field required")
    for name, sha in receipt["tile_sha256"].items():
        if hashlib.sha256((directory / name).read_bytes()).hexdigest() != sha:
            raise ValueError("prepared tile changed")
    image = bpy.data.images.load(
        str(directory / "field.1001.exr"), check_existing=False
    )
    image.source = "TILED"
    image.filepath = str(directory / "field.<UDIM>.exr")
    for index in range(1, 64):
        image.tiles.new(1001 + index)
    image.reload()
    image.colorspace_settings.name = "Non-Color"
    image.alpha_mode = "CHANNEL_PACKED"
    image.use_half_precision = False
    image.name = "isolated positive normal field"
    if (
        not image.is_float
        or image.depth != 128
        or any(tuple(tile.size) != (32, 32) for tile in image.tiles)
    ):
        raise ValueError("complete Float32 source tiles required")
    return image


class TileFields:
    def __init__(self, directory, *, prepare):
        self.directory = Path(directory)
        self.prepare = prepare
        self.images = []

    def __call__(self, values):
        directory = self.directory / f"field-{len(self.images)}"
        if self.prepare:
            write_tiles(values, directory)
        image = load_tiles(values, directory)
        self.images.append(image)
        return image

    @property
    def float_payload_bytes(self):
        return sum(
            tile.size[0] * tile.size[1] * tile.channels * 4
            for image in self.images
            for tile in image.tiles
        )
