"""Whole cached recursive/spatial crowns, same light and leaf geometry."""

import argparse
import json
import time
from pathlib import Path

import bpy
import numpy as np
from blender_instances import instance_group
from fern_material import palette, source_normal
from outshine_ref.rendering.blender_camera import configure_camera
from render_biome_tiles import setup
from render_fern import model
from tree_entry import load


def render(inputs, output, view):
    start = time.perf_counter()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    eye = np.array((0, -22, 11) if view == "front" else (18, -18, 11), float)
    lateral = np.cross(-eye, np.array((0, 0, 1), float))
    lateral /= np.linalg.norm(lateral)
    for location, name in ((-4.0, "recursive"), (4.0, "spatial")):
        folder = inputs / name
        _, products = load(folder, geometry=False)
        rows = products["tables.npz"]
        mats = palette(folder)
        bump = next(n for n in mats[0].node_tree.nodes if n.type == "BUMP")
        mats[0].node_tree.links.new(source_normal(mats[0]), bump.inputs["Normal"])
        wood = model(folder / "wood.npz", mats)
        wood.location = location * lateral
        for variant in range(8):
            mask = rows["leaf_variants"] == variant
            leaf = model(folder / f"blade-{variant}.npz", mats)
            instance_group(
                leaf,
                rows["leaf_origins"][mask] + location * lateral,
                rows["leaf_frames"][mask],
                np.ones(int(mask.sum())),
                name=f"{name}-{variant}",
            )
    target = (0, 0, 3.3)
    scene = setup("Laubwald", {"size_m": 23, "camera": eye.tolist(), "target": target})
    scene.camera = configure_camera(
        {
            "projection": "orthographic",
            "eye": eye.tolist(),
            "target": target,
            "span_m": 21,
        }
    )
    scene.eevee.taa_render_samples = 4
    output.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str((output / f"{view}.png").resolve())
    ready = time.perf_counter()
    print(json.dumps({"stage": "render", "setup_s": ready - start}), flush=True)
    bpy.ops.render.render(write_still=True)
    report = {
        "left": "recursive",
        "right": "spatial",
        "view": view,
        "resolution": [1920, 1080],
        "eye": eye.tolist(),
        "target": target,
        "orthographic_span_m": 21,
        "samples": 4,
        "setup_s": ready - start,
        "render_s": time.perf_counter() - ready,
        "scope": "Same light/material/leaf variants and counts; translations only separate comparison subjects. Different branch architecture, no native/forest/species admission.",
    }
    (output / f"{view}.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--view", choices=("front", "side"), default="front")
    args = parser.parse_args()
    render(args.inputs, args.output, args.view)
