"""Calibrate actual EEVEE linear EXR alpha semantics with known emission/coverage."""

import argparse
import hashlib
import json
from pathlib import Path

import bpy
import gpu
import numpy as np
from blender_preview import material
from twig_light_blender import VIEW, configure_scene


def calibrate(output):
    output.mkdir(parents=True, exist_ok=False)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    right = np.cross((0, 0, 1), VIEW)
    right /= np.linalg.norm(right)
    up = np.cross(VIEW, right)
    vertices = np.array(
        [
            -0.5 * right - 0.25 * up,
            0.5 * right - 0.25 * up,
            0.5 * right + 0.25 * up,
            -0.5 * right + 0.25 * up,
        ]
    )
    mesh = bpy.data.meshes.new("unit known emitter")
    mesh.from_pydata(vertices.tolist(), [], [[0, 1, 2], [0, 2, 3]])
    obj = bpy.data.objects.new("constant quarter coverage", mesh)
    bpy.context.collection.objects.link(obj)
    surface = material("known emission", (0.125, 0.25, 0.5), 1)
    surface.surface_render_method = "DITHERED"
    nodes, links = surface.node_tree.nodes, surface.node_tree.links
    nodes.clear()
    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Color"].default_value = (0.125, 0.25, 0.5, 1)
    transparent = nodes.new("ShaderNodeBsdfTransparent")
    mix = nodes.new("ShaderNodeMixShader")
    mix.inputs[0].default_value = 0.25
    links.new(transparent.outputs[0], mix.inputs[1])
    links.new(emission.outputs[0], mix.inputs[2])
    final = nodes.new("ShaderNodeOutputMaterial")
    links.new(mix.outputs[0], final.inputs["Surface"])
    mesh.materials.append(surface)
    configure_scene(1, 384, VIEW)
    scene = bpy.context.scene
    scene.render.image_settings.file_format = "OPEN_EXR"
    scene.render.image_settings.color_depth = "32"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.filepath = str(output / "linear.exr")
    bpy.ops.render.render(write_still=True)
    image = bpy.data.images.load(str(output / "linear.exr"), check_existing=False)
    rgba = np.empty(2560 * 1440 * 4, np.float32)
    image.pixels.foreach_get(rgba)
    rgba = rgba.reshape(1440, 2560, 4)
    # Fixed central interior excludes edge filtering and uncovered background.
    centre = rgba[690:750, 1220:1340]
    colour = np.array([0.125, 0.25, 0.5])
    premult_error = float(abs(centre[..., :3] - centre[..., 3, None] * colour).max())
    straight_error = float(abs(centre[..., :3] - colour).max())
    assert min(premult_error, straight_error) < 1e-5
    assert abs(float(centre[..., 3].mean()) - 0.25) < 0.01
    result = {
        "known_emission_rgb": colour.tolist(),
        "known_coverage": 0.25,
        "loaded_alpha_mode": image.alpha_mode,
        "maximum_premult_error": premult_error,
        "maximum_straight_error": straight_error,
        "mean_interior_rgba": centre.mean(axis=(0, 1)).tolist(),
        "stored_representation": "premultiplied"
        if premult_error < straight_error
        else "straight",
        "backend": gpu.platform.renderer_get(),
        "implementation_sha256": hashlib.sha256(
            Path(__file__).read_bytes()
        ).hexdigest(),
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    np.savez_compressed(output / "linear.npz", rgba=rgba)
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    calibrate(parser.parse_args().output)
