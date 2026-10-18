"""Equal camera/light/material full and near crown form views, private Blender output."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import bpy
import gpu
import numpy as np
from demand import Camera
from mathutils import Vector
from outshine_ref.rendering.blender_camera import configure_camera
from preview_shadows import configure_preview
from shoot_form import ShootForm
from shoot_form_probe import TRAITS
from shoot_form_scene import leaf_objects, wood_objects
from spatial_group_source import load_source


def environment(camera):
    scene = bpy.context.scene
    configure_preview(scene)
    scene.eevee.taa_render_samples = 32
    scene.camera = configure_camera(
        {
            "projection": "perspective",
            "eye": camera.position,
            "target": camera.target,
            "vertical_fov_deg": camera.fov_y,
            "clip_near_m": camera.near,
            "clip_far_m": camera.far,
            "camera_basis": np.column_stack(
                (camera.axes[0], camera.axes[1], -camera.axes[2])
            ).tolist(),
        }
    )
    scene.render.resolution_x, scene.render.resolution_y = 2560, 1440
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.view_transform = "Standard"
    scene.world = bpy.data.worlds.new("same neutral studio environment")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (
        0.085,
        0.095,
        0.11,
        1,
    )
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.5
    bpy.ops.object.light_add(type="SUN")
    sun = bpy.context.object
    sun.rotation_euler = Vector((0.6, 0.45, -0.7)).to_track_quat("-Z", "Y").to_euler()
    sun.data.energy, sun.data.angle, sun.data.use_shadow = 3.5, 0.05, False
    bpy.ops.object.light_add(type="AREA", location=(0, -12, 15))
    fill = bpy.context.object
    fill.rotation_euler = Vector((0, 12, -6)).to_track_quat("-Z", "Y").to_euler()
    fill.data.energy, fill.data.shape, fill.data.size = 1800, "DISK", 12
    fill.data.use_shadow = False
    return scene


def render(inputs, study, output, *, mode, view):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    source = load_source(inputs)
    config = json.loads((study / "scene.json").read_text())
    c = config[view]
    camera = Camera(
        c["eye"],
        c["target"],
        fov_y=c["fov_y_deg"],
        width=2560,
        height=1440,
        near=0.05,
        far=1000,
    )
    bpy.ops.wm.read_factory_settings(use_empty=True)
    before = time.perf_counter()
    with (
        patch.object(
            ShootForm, "build", side_effect=AssertionError("form build in reader")
        ),
        patch(
            "crown_shoots.CrownShoots.group",
            side_effect=AssertionError("source poses in reader"),
        ),
        patch(
            "numpy.random.Generator", side_effect=AssertionError("randomness in reader")
        ),
    ):
        form = ShootForm.load(study / f"{mode}.npz", expected_source=source.identity)
        leaf_stats = leaf_objects(source, form, camera, inputs, near=view == "near")
        wood_stats = wood_objects(form, camera, study, near=view == "near")
    scene = environment(camera)
    build_s = time.perf_counter() - before
    scene.render.filepath = str(output / f"{view}-{mode}.png")
    before = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "mode": mode,
        "view": view,
        "form_identity": form.identity,
        "source_identity": source.identity,
        "camera": c,
        "resolution": [2560, 1440],
        "samples": 32,
        "backend": {
            "renderer": gpu.platform.renderer_get(),
            "vendor": gpu.platform.vendor_get(),
        },
        "build_s": build_s,
        "render_s": time.perf_counter() - before,
        **leaf_stats,
        **wood_stats,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Equal original procedural study material, lighting and blade resolution within each view. Original C1 body unchanged, demanded matching short-shoot tubes and opaque source blades. Camera-only culling because all light shadows disabled; this is form/normal appearance, not shadow/transport/botanical/LOD/native/device admission.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--study", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mode", choices=TRAITS, required=True)
    parser.add_argument("--view", choices=("whole", "near"), required=True)
    args = parser.parse_args()
    render(args.inputs, args.study, args.output, mode=args.mode, view=args.view)
