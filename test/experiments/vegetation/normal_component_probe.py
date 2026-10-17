"""Actual Blender scalar/rotation controls for the isolated normal-component shader."""

import argparse
import json
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from mathutils import Vector
from normal_component_material import material
from window_portrait_blender import bind_mesh


def run(output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    records = []
    for case in ("flat", "rotated", "mixture"):
        rotation = (
            np.eye(3)
            if case != "rotated"
            else np.array([[0, 0, 1], [0, 1, 0], [-1, 0, 0]])
        )
        vectors = np.zeros((64, 32, 32, 4, 3), np.float32)
        vectors[..., 0, :] = (0.8, 0, 0.6) if case != "mixture" else (0.3, 0, 0.4)
        if case == "mixture":
            vectors[..., 1, :] = (-0.3, 0, 0.4)
        images = np.zeros((64, 32, 32, 9), np.float32)
        images[..., 3] = 1
        images[..., 4:7] = vectors.sum(axis=3)
        for mode in ("mean", "four"):
            began = time.perf_counter()
            bpy.ops.wm.read_factory_settings(use_empty=True)
            surface = material(
                vectors, images, np.tile(np.eye(3), (64, 1, 1)), [1, 0, 0], mode
            )
            vertices = (
                np.array([[-1, -1, 0], [1, -1, 0], [1, 1, 0], [-1, 1, 0]], float)
                @ rotation.T
            )
            uv = np.array([[4, 4], [36, 4], [36, 36], [4, 36]], float) / 320
            bind_mesh(
                "scalar control",
                {
                    "vertices": vertices,
                    "faces": np.array([[0, 1, 2], [0, 2, 3]]),
                    "uv": uv,
                    "weights": np.ones(2),
                },
                surface,
            )
            scene = bpy.context.scene
            scene.render.engine = "BLENDER_EEVEE"
            camera = bpy.data.objects.new(
                "control camera", bpy.data.cameras.new("control camera")
            )
            scene.collection.objects.link(camera)
            scene.camera = camera
            camera.location = Vector(rotation @ np.array([0, 0, 3.0]))
            camera.rotation_euler = (
                (-camera.location).to_track_quat("-Z", "Y").to_euler()
            )
            camera.data.type = "ORTHO"
            camera.data.ortho_scale = 2
            scene.render.resolution_x = scene.render.resolution_y = 64
            scene.render.resolution_percentage = 100
            scene.render.film_transparent = True
            scene.eevee.taa_render_samples = 8
            scene.render.image_settings.file_format = "OPEN_EXR"
            path = output / f"{case}-{mode}.exr"
            scene.render.filepath = str(path)
            bpy.ops.render.render(write_still=True)
            image = bpy.data.images.load(str(path), check_existing=False)
            pixels = np.asarray(image.pixels[:], float).reshape(64, 64, 4)
            expected = (
                (0.3 if mode == "four" else 0.0)
                if case == "mixture"
                else (0.6 if case == "rotated" else 0.8)
            )
            error = float(abs(pixels[8:-8, 8:-8, :3] - expected).max())
            if error > 1e-3:
                raise AssertionError((case, mode, expected, error))
            records.append(
                {
                    "case": case,
                    "mode": mode,
                    "expected_linear": expected,
                    "maximum_error": error,
                    "renderer": gpu.platform.renderer_get(),
                    "seconds": time.perf_counter() - began,
                }
            )
            print(json.dumps(records[-1]), flush=True)
    (output / "report.json").write_text(json.dumps(records, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
