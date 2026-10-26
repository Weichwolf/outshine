"""Actual cached physical grass: sparse raster, then75/150/300 roots per m2."""

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
from blender_preview import mesh_object
from ground_cover_scene import tuft_poses
from outshine_ref.vegetation.graminoid import graminoid_tuft_mesh
from render_biome_tiles import materials, setup


def render(inputs, output):
    began = time.perf_counter()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mats = materials("Laubwald")
    prototypes = []
    for index in range(8):
        mesh = graminoid_tuft_mesh(17 + index, segments=3)
        obj = mesh_object(
            f"grass-{index}",
            mesh["vertices"],
            mesh["faces"],
            mesh["normals"],
            mats[1 + index % 3],
            corner_normals=True,
        )
        obj.data.uv_layers.new(name="physical blade coordinates").data.foreach_set(
            "uv", mesh["uv"][mesh["faces"].ravel()].ravel()
        )
        prototypes.append(obj)
    products = []
    for x, label in (
        (-7.5, "raster"),
        (-2.5, "75.000"),
        (2.5, "150.000"),
        (7.5, "300.000"),
    ):
        path = inputs / f"sites-{label}.npz"
        with np.load(path) as data:
            sites = {key: data[key] for key in data.files}
        poses = tuft_poses(sites)
        poses["origins"][:, 0] += x
        for index, prototype in enumerate(prototypes):
            selected = poses["variants"] == index
            if not selected.any():
                continue
            instance_group(
                prototype,
                poses["origins"][selected],
                poses["frames"][selected],
                poses["scales"][selected],
                name=f"{label}-{index}",
            )
        products.append(
            {
                "label": label,
                "roots": len(sites["xy"]),
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            }
        )
    recipe = {"size_m": 22, "camera": [0, -15, 10], "target": [0, 0, 0.1]}
    scene = setup("Laubwald", recipe)
    scene.camera.data.ortho_scale = 25
    output.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str((output / "grass-comparison.png").resolve())
    prepared = time.perf_counter()
    print(
        json.dumps(
            {"stage": "render", "setup_s": prepared - began, "products": products}
        ),
        flush=True,
    )
    bpy.ops.render.render(write_still=True)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "products_left_to_right": products,
        "camera": recipe,
        "orthographic_width_m": 25,
        "resolution": [1920, 1080],
        "samples": 4,
        "setup_s": prepared - began,
        "render_s": time.perf_counter() - prepared,
        "renderer": gpu.platform.renderer_get(),
        "scope": "Whole4x4m generic grass stands, shared physical prototypes/light/material/camera. Authored densities, not ecological/LOD/runtime admission.",
    }
    (output / "grass-comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    render(args.inputs, args.output)
