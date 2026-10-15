"""Cached whole fern and flat fern/grass understory through actual shared helpers."""

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
from fern_material import attach_normals, palette
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_mesh import mesh_object
from render_biome_tiles import setup


def model(path, mats):
    with np.load(path) as data:
        obj = mesh_object(
            path.stem,
            data["vertices"],
            data["faces"],
            data["normals"],
            mats[0],
            corner_normals=True,
        )
        attach_normals(obj, data["normals"])
        for mat in mats[1:]:
            obj.data.materials.append(mat)
        obj.data.polygons.foreach_set(
            "material_index", data["surface"].astype(np.int32)
        )
        obj.data.uv_layers.new(name="organ UV").data.foreach_set(
            "uv", data["corner_uv"].ravel()
        )
    return obj


def render(inputs, output, case):
    start = time.perf_counter()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mats = palette(inputs)
    hashes = {}

    def digest(path):
        hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()

    for name in (
        "fern-colour.png",
        "fern-height.png",
        "fern-material.json",
        "summary.json",
    ):
        digest(inputs / name)
    if case in ("plant", "field"):
        path = inputs / ("fern-0-near.npz" if case == "plant" else "fern-0-field.npz")
        model(path, mats)
        digest(path)
        scene = setup(
            "Laubwald",
            {"size_m": 1.6, "camera": [1.3, -1.7, 1.1], "target": [0, 0, 0.24]},
        )
        scene.camera = configure_camera(
            {
                "projection": "orthographic",
                "eye": [1.3, -1.7, 1.1],
                "target": [0, 0, 0.24],
                "span_m": 1.7,
            }
        )
        scene.eevee.taa_render_samples = 16
    else:
        for kind, count in (("fern", 4), ("grass", 8)):
            path = inputs / (kind + "-population.npz")
            digest(path)
            with np.load(path) as data:
                poses = {k: data[k] for k in data.files}
            for i in range(count):
                chosen = poses["variants"] == i
                if not chosen.any():
                    continue
                path = inputs / (
                    f"fern-{i}-field.npz" if kind == "fern" else f"grass-{i}.npz"
                )
                obj = model(path, mats)
                digest(path)
                instance_group(
                    obj,
                    poses["origins"][chosen],
                    poses["frames"][chosen],
                    poses["scales"][chosen],
                    name=path.stem,
                )
        scene = setup(
            "Laubwald", {"size_m": 4, "camera": [4, -6, 4], "target": [0, 0, 0.18]}
        )
    output.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str((output / (case + ".png")).resolve())
    ready = time.perf_counter()
    print(
        json.dumps({"stage": "render", "case": case, "setup_s": ready - start}),
        flush=True,
    )
    bpy.ops.render.render(write_still=True)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "case": case,
        "input_hashes": hashes,
        "resolution": [1920, 1080],
        "samples": scene.eevee.taa_render_samples,
        "renderer": gpu.platform.renderer_get(),
        "setup_s": ready - start,
        "render_s": time.perf_counter() - ready,
        "camera_eye_m": list(scene.camera.location),
        "camera_rotation": list(scene.camera.rotation_euler),
        "orthographic_width_m": scene.camera.data.ortho_scale,
        "scope": "Own generic fern/grass study; procedural maps and source CORNER normals into inverse-transpose normal transform and bump, no botanical/native/LOD admission.",
    }
    (output / (case + ".json")).write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--case", choices=("plant", "field", "tile"), required=True)
    args = parser.parse_args()
    render(args.inputs, args.output, args.case)
