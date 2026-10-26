"""Actual one-mesh alpha-order counterexample, not a physical glass model."""

import argparse
import json
from pathlib import Path

import bpy
import numpy as np
from evidence import revision
from mathutils import Vector
from PIL import Image

LAYERS = ((0.0, (0, 0, 1), 0.35), (0.5, (1, 0, 0), 0.55))


def material(method, z, colour, alpha):
    result = bpy.data.materials.new("layer" + str(z))
    result.surface_render_method = method
    tree = result.node_tree
    transparent = tree.nodes.new("ShaderNodeBsdfTransparent")
    emission = tree.nodes.new("ShaderNodeEmission")
    emission.inputs["Color"].default_value = (*colour, 1)
    mixed = tree.nodes.new("ShaderNodeMixShader")
    mixed.inputs[0].default_value = alpha
    tree.links.new(transparent.outputs[0], mixed.inputs[1])
    tree.links.new(emission.outputs[0], mixed.inputs[2])
    tree.links.new(mixed.outputs[0], tree.nodes["Material Output"].inputs["Surface"])
    return result


def draw(output, method, reverse, side):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.threads_mode, scene.render.threads = "FIXED", 2
    scene.eevee.taa_render_samples = 64
    scene.view_settings.view_transform, scene.view_settings.exposure = "Raw", 0
    scene.render.resolution_x = scene.render.resolution_y = 64
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.world = bpy.data.worlds.new("white world")
    background = scene.world.node_tree.nodes["Background"]
    background.inputs["Color"].default_value = (1, 1, 1, 1)
    background.inputs["Strength"].default_value = 1
    points, faces, indices, materials = [], [], [], []
    for z, colour, alpha in LAYERS[::-1] if reverse else LAYERS:
        first = len(points)
        points.extend([(-1, -1, z), (1, -1, z), (1, 1, z), (-1, 1, z)])
        faces.extend([(first, first + 1, first + 2), (first, first + 2, first + 3)])
        indices.extend([len(materials)] * 2)
        materials.append(material(method, z, colour, alpha))
    mesh = bpy.data.meshes.new("one combined mesh")
    mesh.from_pydata(points, [], faces)
    for surface in materials:
        mesh.materials.append(surface)
    mesh.polygons.foreach_set("material_index", indices)
    obj = bpy.data.objects.new("one combined object", mesh)
    scene.collection.objects.link(obj)
    lens = bpy.data.cameras.new("camera")
    lens.type, lens.ortho_scale = "ORTHO", 1
    scene.camera = bpy.data.objects.new("camera", lens)
    eye, target = Vector((0, 0, side * 4)), Vector((0, 0, 0.25))
    scene.camera.location = eye
    scene.camera.rotation_euler = (target - eye).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(scene.camera)
    path = output / f"{method}-{reverse}-{side}.png"
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    with Image.open(path) as image:
        actual = np.array(image.convert("RGB"))[20:44, 20:44].mean((0, 1)) / 255
    expected = np.ones(3)
    for _z, colour, alpha in sorted(LAYERS, key=lambda row: side * row[0]):
        expected = alpha * np.array(colour) + (1 - alpha) * expected
    return {
        "method": method,
        "triangle_order_reversed": reverse,
        "camera_side": side,
        "actual": actual.tolist(),
        "expected_sorted_composite": expected.tolist(),
        "max_error": float(abs(actual - expected).max()),
        "image": str(path),
    }


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the blend-order experiment first")
    output.mkdir(parents=True, exist_ok=True)
    results = []
    for method in ("BLENDED", "DITHERED"):
        for reverse in (False, True):
            for side in (1, -1):
                record = draw(output, method, reverse, side)
                print(json.dumps(record), flush=True)
                results.append(record)
    errors = {
        m: [r["max_error"] for r in results if r["method"] == m]
        for m in ("BLENDED", "DITHERED")
    }
    if max(errors["DITHERED"]) >= 0.01 or sum(e > 0.19 for e in errors["BLENDED"]) != 2:
        raise ValueError(
            "The installed backend no longer reproduces this ordering counterexample"
        )
    (output / "results.json").write_text(
        json.dumps(
            proof
            | {
                "results": results,
                "scope": "Uniform64x64 emitting/transmitting plane unit controls, not a physical glass or architecture showcase. DITHERED mean tolerance0.01 at64samples; not temporal/variance admission. BLENDED is rejected for an unsorted shared mesh.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires an explicit subdirectory of /tmp")
    run(output)
