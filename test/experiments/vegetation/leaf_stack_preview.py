"""Render controlled occluder gaps/materials with the same calibrated leaf adapter."""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from grass_light_preview import camera_depth_receipt, mesh_object, setup, sheet
from leaf_stack_cases import (
    CASES,
    IRRADIANCE,
    LIGHT,
    REFLECTION,
    TRANSMISSION,
    VIEW,
    case_parts,
)
from mathutils import Vector
from PIL import Image
from preview_shadows import configure_preview


def receiver_material(variant):
    receiver = sheet("same calibrated receiver", REFLECTION, TRANSMISSION)
    if variant == "shadow_thickness":
        receiver.use_thickness_from_shadow = True
    elif variant == "flipped_diffuse":
        nodes, links = receiver.node_tree.nodes, receiver.node_tree.links
        nodes.clear()
        output = nodes.new("ShaderNodeOutputMaterial")
        diffuse = nodes.new("ShaderNodeBsdfDiffuse")
        diffuse.inputs["Color"].default_value = (*TRANSMISSION, 1)
        geometry = nodes.new("ShaderNodeNewGeometry")
        negative = nodes.new("ShaderNodeVectorMath")
        negative.operation = "SCALE"
        negative.inputs["Scale"].default_value = -1
        links.new(geometry.outputs["True Normal"], negative.inputs[0])
        links.new(negative.outputs["Vector"], diffuse.inputs["Normal"])
        links.new(diffuse.outputs[0], output.inputs["Surface"])
    return receiver


def populate(variant, clip_end=1000):
    scene = setup(VIEW)
    scene.camera.data.clip_end = clip_end
    receiver = receiver_material(variant)
    absorber = sheet("opaque absorber", np.full(3, 0.02))
    for mat in (receiver, absorber):
        mat.use_backface_culling = False
        mat.use_backface_culling_shadow = False
    for i, (name, _) in enumerate(CASES):
        for part, v, f in case_parts(i):
            mesh_object(
                f"{name}-{part}", v, f, absorber if part == "opaque" else receiver
            )
    bpy.ops.object.light_add(type="SUN")
    lamp = bpy.context.object
    lamp.rotation_euler = (-Vector(LIGHT)).to_track_quat("-Z", "Y").to_euler()
    lamp.data.energy = float(IRRADIANCE.max())
    lamp.data.color = tuple(IRRADIANCE / IRRADIANCE.max())
    lamp.data.angle = 0
    configure_preview(scene)
    scene.eevee.taa_render_samples = 8
    return scene, lamp


def sample_image(target, shadows):
    image = np.array(Image.open(target).convert("RGB"), float) / 255
    expected = TRANSMISSION * abs(LIGHT[0]) * IRRADIANCE / np.pi
    rows = []
    for i, (name, layers) in enumerate(CASES):
        x = int(960 + (i - 4) * 0.14 * 1920 / 1.4)
        q = image[480:600, x - 32 : x + 32]
        linear = np.where(q <= 0.04045, q / 12.92, ((q + 0.055) / 1.055) ** 2.4)
        mean = linear.mean((0, 1))
        row = {
            "name": name,
            "layers": layers,
            "shadows": shadows,
            "mean_linear_rgb": mean.tolist(),
            "unoccluded_expected_rgb": expected.tolist(),
            "green_fraction_of_unoccluded": float(mean[1] / expected[1]),
            "opaque_geometric_visibility": not bool(layers),
        }
        rows.append(row)
    return rows


def scene_settings(scene, lamp):
    return {
        "camera": camera_depth_receipt(scene),
        "sun": {
            key: getattr(lamp.data, key)
            for key in (
                "angle",
                "energy",
                "shadow_buffer_clip_start",
                "shadow_filter_radius",
                "shadow_maximum_resolution",
                "use_shadow_jitter",
            )
        },
        "materials": [
            {
                "name": mat.name,
                "thickness_mode": mat.thickness_mode,
                "use_thickness_from_shadow": mat.use_thickness_from_shadow,
                "use_backface_culling": mat.use_backface_culling,
                "use_backface_culling_shadow": mat.use_backface_culling_shadow,
            }
            for mat in bpy.data.materials
        ],
        "terminator_geometry_offset": [
            obj.shadow_terminator_geometry_offset
            for obj in scene.objects
            if obj.type == "MESH"
        ],
        "terminator_normal_offset": [
            obj.shadow_terminator_normal_offset
            for obj in scene.objects
            if obj.type == "MESH"
        ],
    }


def run(output, variant, clip_end):
    if output.exists():
        raise ValueError("fresh controlled stack output directory required")
    output.mkdir(parents=True)
    scene, lamp = populate(variant, clip_end)
    rows, timings, images = [], [], []
    for shadows in (False, True):
        lamp.data.use_shadow = shadows
        target = output / f"layers-shadow-{int(shadows)}.png"
        scene.render.filepath = str(target)
        began = time.perf_counter()
        bpy.ops.render.render(write_still=True)
        timings.append(time.perf_counter() - began)
        result = sample_image(target, shadows)
        rows.extend(result)
        images.append(
            {
                "name": target.name,
                "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
            }
        )
        print(json.dumps(result), flush=True)
    controls = [r for r in rows if not r["shadows"] or not r["layers"]]
    clear_error = max(
        np.max(abs(np.array(r["mean_linear_rgb"]) - r["unoccluded_expected_rgb"]))
        for r in controls
    )
    opaque_error = max(
        max(r["mean_linear_rgb"])
        for r in rows
        if r["shadows"] and any(layer[1] == "opaque" for layer in r["layers"])
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "blender": bpy.app.version_string,
        "renderer": gpu.platform.renderer_get(),
        "profile": [1920, 1080],
        "samples": 8,
        "variant": variant,
        "clip_end_m": clip_end,
        "camera": camera_depth_receipt(scene),
        "settings": scene_settings(scene, lamp),
        "view": VIEW.tolist(),
        "light": LIGHT.tolist(),
        "receiver_rt": [*REFLECTION.tolist(), *TRANSMISSION.tolist()],
        "shadow_backface_culling": False,
        "wall_s": timings,
        "images": images,
        "cases": rows,
        "qualification": {
            "unoccluded_sheet_max_abs_rgb_error": float(clear_error),
            "unoccluded_sheet_pass": bool(clear_error <= 0.005),
            "opaque_layer_max_abs_rgb_error": float(opaque_error),
            "opaque_layer_pass": bool(opaque_error <= 0.005),
            "preset_abs_rgb_limit": 0.005,
        },
        "scope": "Isolated direct-light/opaque-expectation diagnosis; no assumption that hemispherical leaf T is a beam transmission coefficient",
    }
    (output / "result.json").write_text(json.dumps(report, indent=2))
    if clear_error > 0.005:
        raise ValueError(
            "existing0.005 RGB isolated-sheet criterion failed before layer interpretation"
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--clip-end", type=float, default=1000)
    parser.add_argument(
        "--variant",
        choices=("baseline", "shadow_thickness", "flipped_diffuse"),
        default="baseline",
    )
    args = parser.parse_args()
    run(args.output, args.variant, args.clip_end)
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)
