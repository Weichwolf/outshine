"""Same complete wood, camera and light with constant versus metric bark shading."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from demand import Camera
from limb_bark_material import mature_material
from outshine_ref.rendering.blender_mesh import mesh_object
from render_shoot_forms import environment
from shoot_form_scene import constant_material


def render(inputs, output, mode, view):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    recipe = json.loads((inputs / "material.json").read_text())
    material = (
        mature_material(recipe)
        if mode == "bark"
        else constant_material("original smooth wood", (0.12, 0.065, 0.025), 0.8)
    )
    with np.load(inputs / "wood-bark.npz", allow_pickle=False) as mesh:
        obj = mesh_object(
            "same complete mature C1 wood",
            mesh["vertices"],
            mesh["faces"],
            mesh["normals"],
            material,
            True,
        )
        if mode == "bark":
            for name, kind, field in (
                ("bark_coordinates", "FLOAT_VECTOR", "vector"),
                ("bark_radius", "FLOAT", "value"),
            ):
                attribute = obj.data.attributes.new(name, kind, "POINT")
                attribute.data.foreach_set(field, mesh[name].astype(np.float32).ravel())
    eye, target = (
        ((0, -26, 10), (0, 0, 9)) if view == "whole" else ((1.5, -3, 1.8), (0, 0, 1.5))
    )
    camera = Camera(
        eye,
        target,
        fov_y=50 if view == "whole" else 45,
        width=2560,
        height=1440,
        near=0.05,
        far=1000,
    )
    scene = environment(camera)
    scene.render.filepath = str(output / "wood.png")
    start = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    report = {
        "mode": mode,
        "view": view,
        "eye": eye,
        "target": target,
        "resolution": [2560, 1440],
        "samples": 32,
        "renderer": gpu.platform.renderer_get(),
        "render_s": time.perf_counter() - start,
        "wood_npz_sha256": hashlib.sha256(
            (inputs / "wood-bark.npz").read_bytes()
        ).hexdigest(),
        "material": recipe if mode == "bark" else "original constant",
        "png_sha256": hashlib.sha256((output / "wood.png").read_bytes()).hexdigest(),
        "scope": "Complete identical wood; canopy omitted equally to expose material. Static controlled light, no shadows. Bump only, no silhouette displacement or fused collars.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mode", choices=("smooth", "bark"), required=True)
    parser.add_argument("--view", choices=("whole", "trunk"), required=True)
    args = parser.parse_args()
    render(args.inputs, args.output, args.mode, args.view)
