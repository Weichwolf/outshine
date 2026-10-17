"""Counterprobe for accidental cross-view mip filtering in the diagnostic atlas."""

import argparse
import json
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector
from normal_component_material import material
from window_portrait_blender import bind_mesh


def run(output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    results = []
    for interpolation in ("Linear", "Closest"):
        for footprint in (64, 1):
            bpy.ops.wm.read_factory_settings(use_empty=True)
            frames = np.tile(np.diag([-1.0, 1.0, -1.0]), (64, 1, 1))
            frames[0] = np.eye(3)
            vectors = np.zeros((64, 32, 32, 4, 3), np.float32)
            vectors[..., 0, 2] = -1
            vectors[0, ..., 0, 2] = 1
            images = np.zeros((64, 32, 32, 9), np.float32)
            images[..., 3] = 1
            images[..., 6] = 1
            surface = material(vectors, images, frames, [0, 0, 1], "four")
            for node in surface.node_tree.nodes:
                if node.type == "TEX_IMAGE":
                    node.interpolation = interpolation
            bind_mesh(
                "same view0 control",
                {
                    "vertices": np.array(
                        [[-1, -1, 0], [1, -1, 0], [1, 1, 0], [-1, 1, 0]], float
                    ),
                    "faces": np.array([[0, 1, 2], [0, 2, 3]]),
                    "uv": np.array([[4, 4], [36, 4], [36, 36], [4, 36]], float) / 320,
                    "weights": np.ones(2),
                },
                surface,
            )
            scene = bpy.context.scene
            scene.render.engine = "BLENDER_EEVEE"
            camera = bpy.data.objects.new("camera", bpy.data.cameras.new("camera"))
            scene.collection.objects.link(camera)
            scene.camera = camera
            camera.location = Vector([0, 0, 3])
            camera.rotation_euler = (
                (-camera.location).to_track_quat("-Z", "Y").to_euler()
            )
            camera.data.type = "ORTHO"
            camera.data.ortho_scale = 128 / footprint
            scene.render.resolution_x = scene.render.resolution_y = 64
            scene.render.resolution_percentage = 100
            scene.render.film_transparent = True
            scene.eevee.taa_render_samples = 32
            scene.render.image_settings.file_format = "OPEN_EXR"
            scene.render.image_settings.color_depth = "32"
            path = output / f"{interpolation}-{footprint}.exr"
            scene.render.filepath = str(path)
            bpy.ops.render.render(write_still=True)
            image = bpy.data.images.load(str(path), check_existing=False)
            pixels = np.asarray(image.pixels[:], float).reshape(64, 64, 4)
            alpha = pixels[..., 3].sum()
            response = float(pixels[..., 0].sum() / alpha)
            result = {
                "interpolation": interpolation,
                "footprint_pixels": footprint,
                "expected_conditional_response": 1,
                "measured_response": response,
                "alpha_sum": float(alpha),
                "passes": abs(response - 1) < 0.003,
            }
            results.append(result)
            print(json.dumps(result), flush=True)
    (output / "report.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
