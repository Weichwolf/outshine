"""Actual cached flowering plant and equal-model-budget rooted stand views."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from blender_instances import instance_group
from blender_preview import material
from mathutils import Vector
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_light import configure_daylight
from outshine_ref.rendering.blender_mesh import mesh_object
from preview_shadows import configure_preview
from render_biome_tiles import materials, setup


def palette():
    result = materials("Laubwald")
    for name, color, roughness in (
        ("green flowering stalk", (0.035, 0.12, 0.015), 0.6),
        ("broad basal leaf", (0.035, 0.16, 0.024), 0.48),
        ("thin ivory corolla", (0.72, 0.76, 0.63), 0.48),
    ):
        mat = material(name, color, roughness)
        nodes, links = mat.node_tree.nodes, mat.node_tree.links
        thin = nodes.new("ShaderNodeBsdfTranslucent")
        thin.inputs["Color"].default_value = (*color, 1)
        mix = nodes.new("ShaderNodeMixShader")
        mix.inputs[0].default_value = 0.12
        links.new(nodes["Principled BSDF"].outputs[0], mix.inputs[1])
        links.new(thin.outputs[0], mix.inputs[2])
        links.new(mix.outputs[0], nodes["Material Output"].inputs["Surface"])
        result.append(mat)
    return result


def model(path, mats):
    with np.load(path) as data:
        mesh = {key: data[key] for key in data.files}
    obj = mesh_object(
        path.stem,
        mesh["vertices"],
        mesh["faces"],
        mesh["normals"],
        mats[0],
        corner_normals=True,
    )
    for mat in mats[1:]:
        obj.data.materials.append(mat)
    obj.data.polygons.foreach_set("material_index", mesh["surface"].astype(np.int32))
    obj.data.uv_layers.new(name="physical organ UV").data.foreach_set(
        "uv", mesh["uv"].ravel()
    )
    return obj


def hero_scene():
    scene = bpy.context.scene
    configure_preview(scene)
    scene.render.resolution_x, scene.render.resolution_y = 1920, 1080
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.eevee.taa_render_samples = 16
    configure_daylight(scene)
    bpy.ops.mesh.primitive_plane_add(size=2)
    bpy.context.object.data.materials.append(
        material("neutral soil", (0.06, 0.075, 0.025), 0.95)
    )
    bpy.ops.object.light_add(type="AREA", location=(-0.6, -1, 1.5))
    bpy.context.object.data.energy, bpy.context.object.data.size = 12, 1.4
    bpy.context.object.rotation_euler = (
        Vector((0.6, 1, -1.4)).to_track_quat("-Z", "Y").to_euler()
    )
    scene.camera = configure_camera(
        {
            "projection": "orthographic",
            "eye": [0.36, -0.65, 0.38],
            "target": [0, -0.025, 0.11],
            "span_m": 0.52,
        }
    )
    return scene


def render(inputs, output, case):
    start = time.perf_counter()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mats, hashes = palette(), {}
    if case == "plant":
        path = inputs / "convallaria-near.npz"
        model(path, mats)
        hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        scene = hero_scene()
    else:
        summary = json.loads((inputs / "comparison.json").read_text())
        path = inputs / (case + ".npz")
        with np.load(path) as data:
            poses = {key: data[key] for key in data.files}
        hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        for row in summary["models"]:
            selected = (poses["classes"] == summary["names"].index(row["form"])) & (
                poses["variants"] == row["variant"]
            )
            if not selected.any():
                continue
            path = inputs / row["file"]
            prototype = model(path, mats)
            hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
            instance_group(
                prototype,
                poses["origins"][selected],
                poses["frames"][selected],
                poses["scales"][selected],
                name=path.stem,
            )
        scene = setup(
            "Laubwald", {"size_m": 8, "camera": [8, -11, 7], "target": [0, 0, 0.25]}
        )
    output.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str((output / (case + ".png")).resolve())
    prepared = time.perf_counter()
    print(
        json.dumps({"stage": "render", "case": case, "setup_s": prepared - start}),
        flush=True,
    )
    bpy.ops.render.render(write_still=True)
    report = {
        "case": case,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_hashes": hashes,
        "resolution": [1920, 1080],
        "samples": 16 if case == "plant" else 4,
        "renderer": gpu.platform.renderer_get(),
        "setup_s": prepared - start,
        "render_s": time.perf_counter() - prepared,
        "camera_eye_m": list(scene.camera.location),
        "camera_rotation": list(scene.camera.rotation_euler),
        "orthographic_width_m": scene.camera.data.ortho_scale,
        "scope": "Own Convallaria study, generic grass height forms, no botanical/native/LOD admission. Shared light/material/camera and identical intrinsic model budget across shuffled/grouped stands.",
    }
    (output / (case + ".json")).write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {key: value for key, value in report.items() if key != "input_hashes"}
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--case", choices=("plant", "grouped", "shuffled"), required=True
    )
    args = parser.parse_args()
    render(args.inputs, args.output, args.case)
