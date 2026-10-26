"""Shared neutral opaque PBR comparison; native procedural materials remain unqualified."""

import argparse
import hashlib
import json
import math
import os
import subprocess
import sys
from pathlib import Path

import bpy
import numpy as np
from outshine_ref.rendering.blender_camera import configure_camera, verify_camera
from outshine_ref.rendering.blender_light import configure_daylight


def material(name, colour=None):
    result = bpy.data.materials.new(name)
    result.use_nodes = True
    principled = result.node_tree.nodes.get("Principled BSDF")
    principled.inputs["Metallic"].default_value = 0
    principled.inputs["Roughness"].default_value = 0.7
    if colour is None:
        attribute = result.node_tree.nodes.new("ShaderNodeVertexColor")
        attribute.layer_name = "source colour"
        result.node_tree.links.new(
            attribute.outputs["Color"], principled.inputs["Base Color"]
        )
    else:
        principled.inputs["Base Color"].default_value = (*colour, 1)
    return result


def build_mesh(scene, arrays):
    mesh = bpy.data.meshes.new("source geometry")
    mesh.from_pydata(arrays["positions"].tolist(), [], arrays["indices"].tolist())
    mesh.update()
    colours = mesh.color_attributes.new(
        name="source colour", type="FLOAT_COLOR", domain="POINT"
    )
    colours.data.foreach_set("color", arrays["colour"].ravel())
    mesh.normals_split_custom_set_from_vertices(arrays["shading_normal"].tolist())
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    obj = bpy.data.objects.new("source geometry", mesh)
    scene.collection.objects.link(obj)
    mesh.materials.append(material("same neutral source PBR"))


def lighting(scene, profile, ground):
    settings = configure_daylight(scene)
    sun = next(obj for obj in scene.objects if obj.type == "LIGHT")
    if profile == "back":
        sun.rotation_euler.z += math.pi
    if profile == "night":
        sun.data.energy = 0.015
        scene.world.node_tree.nodes["Background"].inputs[
            "Strength"
        ].default_value = 0.004
        point = bpy.data.lights.new("shared nocturnal area", "AREA")
        point.energy, point.size = 20000, 12
        obj = bpy.data.objects.new("shared nocturnal area", point)
        obj.location = (0, 0, ground["z"] + 40)
        scene.collection.objects.link(obj)
    return settings | {
        "comparison_profile": profile,
        "actual_sun_rotation": list(sun.rotation_euler),
        "actual_sun_energy": sun.data.energy,
        "actual_world_strength": scene.world.node_tree.nodes["Background"]
        .inputs["Strength"]
        .default_value,
    }


def run(args):
    scene_data = json.loads(args.scene.read_text())
    args.output.mkdir(parents=True, exist_ok=False)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = (
        "BLENDER_EEVEE" if bpy.app.version >= (5, 0, 0) else "BLENDER_EEVEE_NEXT"
    )
    scene.render.threads_mode, scene.render.threads = "FIXED", 2
    scene.eevee.taa_render_samples = 16
    scene.render.resolution_x, scene.render.resolution_y = (
        scene_data["width"],
        scene_data["height"],
    )
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.film_transparent = False
    mesh_path = args.scene.parent / scene_data["mesh"]
    with np.load(mesh_path, allow_pickle=False) as data:
        arrays = {k: data[k] for k in data.files}
    build_mesh(scene, arrays)
    ground = scene_data["ground"]
    bpy.ops.mesh.primitive_plane_add(size=ground["span"], location=(0, 0, ground["z"]))
    bpy.context.object.data.materials.append(
        material("shared neutral ground", (0.2, 0.2, 0.2))
    )
    light = lighting(scene, args.lighting, ground)
    rows = np.array(scene_data["camera_basis"])
    eye = np.array(scene_data["eye"])
    width, height = scene_data["width"], scene_data["height"]
    camera_config = {
        "projection": "perspective",
        "eye": eye.tolist(),
        "target": (eye + 1000 * rows[2]).tolist(),
        "camera_basis": np.column_stack((rows[0], rows[1], -rows[2])).tolist(),
        "vertical_fov_deg": math.degrees(
            2 * math.atan(height / (2 * scene_data["focal_px"]))
        ),
        "clip_near_m": 0.1,
        "clip_far_m": 240000,
        "profile": [width, height],
        "id": "scene",
        "output": str(args.output / "scene.png"),
    }
    camera = configure_camera(camera_config)
    scene.camera = camera
    bpy.context.view_layer.update()
    projection = np.array(
        camera.calc_matrix_camera(
            bpy.context.evaluated_depsgraph_get(), x=width, y=height
        )
    )
    view = np.array(camera.matrix_world.inverted())
    proof = verify_camera(camera_config, projection, view, arrays["positions"])
    scene.render.filepath = str(args.output / "scene.png")
    bpy.ops.render.render(write_still=True)
    report = {
        "geometry_producer": scene_data["producer"],
        "renderer_producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "renderer_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "scene_sha256": hashlib.sha256(args.scene.read_bytes()).hexdigest(),
        "mesh_sha256": hashlib.sha256(mesh_path.read_bytes()).hexdigest(),
        "profile": [width, height],
        "samples": 16,
        "blender": bpy.app.version_string,
        "engine": scene.render.engine,
        "light": light,
        "camera": proof,
        "triangles": len(arrays["indices"]),
        "scope": scene_data["scope"]
        + " No emissive-window state or native procedural wall/roof shader; common source-colour neutral PBR isolates geometric/normal/light effects.",
    }
    (args.output / "render.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--lighting", choices=["day", "back", "night"], default="day")
    run(parser.parse_args())
    # Existing bpy teardown workaround, after the image and report are flushed.
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)
