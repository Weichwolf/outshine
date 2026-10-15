"""Actual whole existing tree reference from complete saved products, not a canopy proxy."""

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
from fern_material import palette, source_normal
from outshine_ref.rendering.blender_camera import configure_camera
from render_biome_tiles import setup
from render_fern import model
from tree_entry import load


def render(inputs, output, view):
    began = time.perf_counter()
    metadata, products = load(inputs, geometry=False)
    rows = products["tables.npz"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mats = palette(inputs)
    bark = mats[0]
    bump = next(n for n in bark.node_tree.nodes if n.type == "BUMP")
    bark.node_tree.links.new(source_normal(bark), bump.inputs["Normal"])
    model(inputs / "wood.npz", mats)
    for i in range(8):
        mask = rows["leaf_variants"] == i
        obj = model(inputs / f"blade-{i}.npz", mats)
        instance_group(
            obj,
            rows["leaf_origins"][mask],
            rows["leaf_frames"][mask],
            np.ones(int(mask.sum())),
            name=f"stored leaf variant {i}",
        )
    eye, target, width = (3, -4, 2.7), (0, 0, 0.95), 3.5
    if view == "detail":
        eye, target, width = (1.7, -2.0, 1.9), (0.4, -0.1, 1.35), 1.5
    scene = setup("Laubwald", {"size_m": 4, "camera": eye, "target": target})
    scene.camera = configure_camera(
        {"projection": "orthographic", "eye": eye, "target": target, "span_m": width}
    )
    scene.eevee.taa_render_samples = 16
    output.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str((output / (view + ".png")).resolve())
    ready = time.perf_counter()
    print(json.dumps({"stage": "render", "setup_s": ready - began}), flush=True)
    bpy.ops.render.render(write_still=True)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "view": view,
        "source_plan": metadata["plan_identity"],
        "resolution": [1920, 1080],
        "samples": 16,
        "renderer": gpu.platform.renderer_get(),
        "setup_s": ready - began,
        "render_s": time.perf_counter() - ready,
        "input_hashes": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(inputs.iterdir())
            if p.is_file()
        },
        "scope": metadata["scope"],
    }
    (output / (view + ".json")).write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps({k: v for k, v in result.items() if k != "input_hashes"}), flush=True
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--view", choices=("whole", "detail"), default="whole")
    args = parser.parse_args()
    render(args.inputs, args.output, args.view)
