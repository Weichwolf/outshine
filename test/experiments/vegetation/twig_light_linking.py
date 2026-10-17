"""Actual EEVEE shadow-linking counter: a blocker belongs only to a red sun."""

import argparse
import hashlib
import json
from pathlib import Path

import bpy
import gpu
import numpy as np
from blender_preview import material
from organ_preview import link_shadow_products
from twig_light_blender import VIEW, configure_scene


def plane(name, scale, distance, surface):
    right = np.cross((0, 0, 1), VIEW)
    right /= np.linalg.norm(right)
    up = np.cross(VIEW, right)
    vertices = (
        np.array(
            [
                -0.5 * right - 0.25 * up,
                0.5 * right - 0.25 * up,
                0.5 * right + 0.25 * up,
                -0.5 * right + 0.25 * up,
            ]
        )
        * scale
        + VIEW * distance
    )
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices.tolist(), [], [[0, 1, 2], [0, 2, 3]])
    mesh.materials.append(surface)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def scene(mode):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    receiver = plane("white receiver", 1, 0, material("white", (1, 1, 1), 1))
    receiver["vegetation_pass_role"] = "all"
    surface = material("shadow-only blocker", (1, 1, 1), 1)
    surface.surface_render_method = "DITHERED"
    nodes, links = surface.node_tree.nodes, surface.node_tree.links
    transparent = nodes.new("ShaderNodeBsdfTransparent")
    path = nodes.new("ShaderNodeLightPath")
    mix = nodes.new("ShaderNodeMixShader")
    links.new(path.outputs["Is Shadow Ray"], mix.inputs[0])
    links.new(transparent.outputs[0], mix.inputs[1])
    links.new(nodes.get("Principled BSDF").outputs[0], mix.inputs[2])
    links.new(mix.outputs[0], nodes.get("Material Output").inputs["Surface"])
    blocker = plane("red-sun blocker", 0.5, 0.15, surface)
    blocker["vegetation_pass_role"] = "shadow"
    blocker["shadow_light"] = "sun"
    configure_scene(1, 384, VIEW)
    red = next(o for o in bpy.context.scene.objects if o.type == "LIGHT")
    red.data.color = (1, 0, 0)
    bpy.ops.object.light_add(type="SUN")
    green = bpy.context.object
    green.rotation_euler = red.rotation_euler
    green.data.energy, green.data.angle, green.data.color = 1, 0.01, (0, 1, 0)
    if mode in ("linked", "owned", "both"):
        if mode == "owned":
            blocker["vegetation_pass_role"] = "all"
            blocker["shadow_light"] = "fill"
            blocker["shadow_lights"] = {"sun": True}
        elif mode == "both":
            blocker["shadow_lights"] = {"sun": True, "fill": True}
        link_shadow_products(red, "sun")
        link_shadow_products(green, "fill")
    elif mode == "clear":
        blocker.hide_render = True
    elif mode != "all":
        raise ValueError("declared all/linked/clear counter required")
    return {
        name: []
        if lamp.light_linking.blocker_collection is None
        else sorted(o.name for o in lamp.light_linking.blocker_collection.objects)
        for name, lamp in (("sun", red), ("fill", green))
    }


def run(output, modes=("all", "linked", "clear")):
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    for mode in modes:
        links = scene(mode)
        current = bpy.context.scene
        current.render.image_settings.file_format = "OPEN_EXR"
        current.render.image_settings.color_mode = "RGBA"
        current.render.image_settings.color_depth = "32"
        path = output / f"{mode}.exr"
        current.render.filepath = str(path)
        bpy.ops.render.render(write_still=True)
        image = bpy.data.images.load(str(path), check_existing=False)
        rgba = np.empty(2560 * 1440 * 4, np.float32)
        image.pixels.foreach_get(rgba)
        rgba = rgba.reshape(1440, 2560, 4)
        centre = rgba[700:740, 1240:1320]
        record = {
            "mode": mode,
            "collections": links,
            "central_mean_rgba": centre.mean(axis=(0, 1), dtype=np.float64).tolist(),
            "backend": gpu.platform.renderer_get(),
        }
        rows.append(record)
        np.savez_compressed(output / f"{mode}.npz", rgba=rgba)
        print(json.dumps(record), flush=True)
    result = {
        "cases": rows,
        "implementation_sha256": hashlib.sha256(
            Path(__file__).read_bytes()
        ).hexdigest(),
        "link_helper_sha256": hashlib.sha256(
            Path(__file__).with_name("organ_preview.py").read_bytes()
        ).hexdigest(),
        "scope": "Actual two-sun EEVEE test of the existing blocker-collection helper. No native/world or general physical-lighting admission.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--modes",
        choices=("all", "linked", "clear", "owned", "both"),
        nargs="+",
        default=("all", "linked", "clear"),
    )
    args = parser.parse_args()
    run(args.output, args.modes)
