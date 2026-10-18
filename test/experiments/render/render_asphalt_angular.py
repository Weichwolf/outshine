"""Whole material plates from cached scalar fields; Blender research, not native MR parity."""

import argparse
import hashlib
import json
import resource
import sys
import tempfile
import time
from pathlib import Path
from unittest.mock import patch

import bpy
import gpu
import numpy as np
from asphalt_field import parameters
from compare_asphalt_angular import ROOT, recipes
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_light import configure_daylight
from preview_shadows import configure_preview


def plate(name, field, base, roughness, x):
    colour, rough = parameters(field, base, roughness)
    pixels = np.dstack((colour, rough)).astype("<f4")
    size = len(field)
    image = bpy.data.images.new(name, size, size, alpha=True, float_buffer=True)
    image.colorspace_settings.name = "Non-Color"
    image.alpha_mode = "CHANNEL_PACKED"
    image.pixels.foreach_set(pixels.ravel())
    image.update()
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    shader = nodes.get("Principled BSDF")
    texture = nodes.new("ShaderNodeTexImage")
    texture.image, texture.extension, texture.interpolation = image, "REPEAT", "Linear"
    material.node_tree.links.new(texture.outputs["Color"], shader.inputs["Base Color"])
    material.node_tree.links.new(texture.outputs["Alpha"], shader.inputs["Roughness"])
    shader.inputs["Metallic"].default_value = 0
    bpy.ops.mesh.primitive_plane_add(size=1, location=(x, 0, 0.003))
    obj = bpy.context.object
    obj.name, obj.scale = name, (0.4, 0.65, 1)
    for loop in obj.data.uv_layers.active.data:
        loop.uv.x *= 0.4 / 0.384
        loop.uv.y *= 0.65 / 0.384
    obj.data.materials.append(material)
    return pixels.nbytes


def setup_scene():
    scene = bpy.context.scene
    configure_preview(scene)
    scene.eevee.taa_render_samples = 32
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    camera = {
        "projection": "perspective",
        "eye": [0, -1.15, 1.6],
        "target": [0, 0, 0],
        "vertical_fov_deg": 34,
        "clip_near_m": 0.01,
        "clip_far_m": 20,
    }
    scene.camera = configure_camera(camera)
    lighting = configure_daylight(scene)
    bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, 0))
    floor = bpy.data.materials.new("neutral studio ground")
    floor.diffuse_color = (0.16, 0.16, 0.16, 1)
    bpy.context.object.data.materials.append(floor)
    return scene, camera, lighting


def render(inputs, output):
    fields, hashes = [], {}
    for label, filename in (
        ("periodic Gaussian", "gaussian.npz"),
        ("angular one site", "density-1.npz"),
        ("angular four sites", "density-4.npz"),
    ):
        path = inputs / filename
        with np.load(path, allow_pickle=False) as stored:
            fields.append((label, stored["field_0"]))
        hashes[filename] = hashlib.sha256(path.read_bytes()).hexdigest()
    reports = []
    for recipe, base, rough in recipes():
        bpy.ops.wm.read_factory_settings(use_empty=True)
        with (
            patch(
                "asphalt_angular.angular_tile",
                side_effect=AssertionError("new bake in cache reader"),
            ),
            patch(
                "asphalt_angular.aggregates",
                side_effect=AssertionError("new source in cache reader"),
            ),
        ):
            copied = sum(
                plate(label, field, base, rough, x)
                for (label, field), x in zip(fields, (-0.46, 0, 0.46), strict=True)
            )
        scene, camera, lighting = setup_scene()
        scene.render.filepath = str(output / f"{recipe}.png")
        started = time.perf_counter()
        bpy.ops.render.render(write_still=True)
        reports.append(
            {
                "recipe": recipe,
                "render_seconds": time.perf_counter() - started,
                "camera": camera,
                "lighting": lighting,
                "plate_order_left_to_right": [x[0] for x in fields],
                "plate_size_m": [0.4, 0.65],
                "source_decoded_bytes": sum(x.nbytes for _, x in fields),
                "explicit_rgba32f_write_bytes": copied,
            }
        )
    (output / "report.json").write_text(
        json.dumps(
            {
                "input_sha256": hashes,
                "frames": reports,
                "resolution": [1280, 720],
                "samples": 32,
                "bpy_version": bpy.app.version_string,
                "backend": gpu.platform.renderer_get(),
                "peak_process_rss_bytes": resource.getrusage(
                    resource.RUSAGE_SELF
                ).ru_maxrss
                * (1 if sys.platform == "darwin" else 1024),
                "scope": "Blender plates show cached scalar-source shape with same recipe/camera/daylight. Base levels uploaded as RGBA32F; Blender sampling, not the exported mip chain. No native GLSL BRDF, world/Place, UV/role/cache integration or device-budget admission.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    if (
        target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(ROOT)
        or target.exists()
    ):
        parser.error(
            "fresh explicit system-temp render output outside the checkout required"
        )
    target.mkdir(parents=True)
    render(args.inputs.resolve(), target)
