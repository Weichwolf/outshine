"""Matched real lit box/masked-plane controls; native shading/timing stays separate."""

import argparse
import hashlib
import json
import math
import subprocess
from pathlib import Path

import bpy
import numpy as np
from blender_urban_masks import install
from outshine_ref.rendering.blender_camera import configure_camera, verify_camera
from outshine_ref.rendering.blender_light import configure_daylight
from urban_patches import load


def light(scene, profile):
    settings = configure_daylight(scene)
    sun = next(obj for obj in scene.objects if obj.type == "LIGHT")
    if profile == "back":
        sun.rotation_euler.z += math.pi
    if profile == "night":
        sun.data.energy = 0.015
        scene.world.node_tree.nodes["Background"].inputs[
            "Strength"
        ].default_value = 0.004
        area = bpy.data.lights.new("shared local night light", "AREA")
        area.energy, area.size = 20000, 12
        obj = bpy.data.objects.new("shared local night light", area)
        obj.location = (0, 0, 40)
        scene.collection.objects.link(obj)
    return settings | {
        "profile": profile,
        "sun_rotation": list(sun.rotation_euler),
        "sun_energy": sun.data.energy,
    }


def run(args):
    specification = json.loads(args.scene.read_text())
    args.output.mkdir(parents=True, exist_ok=False)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = (
        "BLENDER_EEVEE" if bpy.app.version >= (5, 0, 0) else "BLENDER_EEVEE_NEXT"
    )
    scene.render.threads_mode, scene.render.threads = "FIXED", 2
    scene.eevee.taa_render_samples = 32
    scene.render.resolution_x, scene.render.resolution_y = (
        specification["width"],
        specification["height"],
    )
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    product_path = args.scene.parent / specification["product"]
    mesh_path = args.scene.parent / specification["mesh"]
    product = load(product_path)
    with np.load(mesh_path, allow_pickle=False) as archive:
        arrays = {key: archive[key] for key in archive.files}
    geometry = install(
        scene,
        arrays,
        product,
        specification["chunks"],
        specification["mode"] == "masks",
    )
    bpy.ops.mesh.primitive_plane_add(size=specification["ground_span"])
    ground = bpy.data.materials.new("same neutral ground")
    ground.use_nodes = True
    ground.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (
        0.2,
        0.2,
        0.2,
        1,
    )
    ground.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.7
    bpy.context.object.data.materials.append(ground)
    lighting = light(scene, args.lighting)
    right, up, forward = np.array(specification["camera_basis"])
    eye = np.array(specification["eye"])
    camera_data = {
        "projection": "perspective",
        "eye": eye.tolist(),
        "target": (eye + forward * 1000).tolist(),
        "camera_basis": np.column_stack((right, up, -forward)).tolist(),
        "vertical_fov_deg": math.degrees(
            2 * math.atan(specification["height"] / (2 * specification["focal_px"]))
        ),
        "clip_near_m": 0.1,
        "clip_far_m": 240000,
        "profile": [specification["width"], specification["height"]],
        "id": "scene",
        "output": str(args.output / "scene.png"),
    }
    camera = configure_camera(camera_data)
    scene.camera = camera
    bpy.context.view_layer.update()
    projection = np.array(
        camera.calc_matrix_camera(
            bpy.context.evaluated_depsgraph_get(),
            x=specification["width"],
            y=specification["height"],
        )
    )
    proof = verify_camera(
        camera_data,
        projection,
        np.array(camera.matrix_world.inverted()),
        arrays["positions"],
    )
    scene.render.filepath = str(args.output / "scene.png")
    bpy.ops.render.render(write_still=True)
    report = {
        "producer": specification["producer"],
        "renderer_producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "scene_sha256": hashlib.sha256(args.scene.read_bytes()).hexdigest(),
        "product_sha256": hashlib.sha256(product_path.read_bytes()).hexdigest(),
        "mesh_sha256": hashlib.sha256(mesh_path.read_bytes()).hexdigest(),
        "renderer_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "profile": [specification["width"], specification["height"]],
        "samples": 32,
        "blender": bpy.app.version_string,
        "geometry": geometry,
        "light": lighting,
        "camera": proof,
        "study": specification["study"],
        "scope": "Actual shared neutral PBR for same inferred geometry. No native procedural materials, filtering/TAA/alpha/caster/reflection/device admission; not actual detailed-world reconstruction.",
    }
    (args.output / "render.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--lighting", choices=["day", "back", "night"], required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
