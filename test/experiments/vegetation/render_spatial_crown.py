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
    study = json.loads((inputs / "comparison.json").read_text())
    presentation = study.get("presentation_labels", {})
    camera = study.get("presentation_camera", {})
    bpy.ops.wm.read_factory_settings(use_empty=True)
    eye = np.array(
        camera.get("eye", (0, -22, 11) if view == "front" else (18, -18, 11)), float
    )
    lateral = np.cross(-eye, np.array((0, 0, 1), float))
    lateral /= np.linalg.norm(lateral)
    separation = camera.get("separation_m", 4.0)
    for location, name in ((-separation, "recursive"), (separation, "spatial")):
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
    target = camera.get("target", (0, 0, 3.3))
    span = camera.get("span_m", 21)
    scene = setup(
        "Laubwald",
        {
            "size_m": camera.get("ground_size_m", 23),
            "camera": eye.tolist(),
            "target": target,
        },
    )
    scene.camera = configure_camera(
        {
            "projection": "orthographic",
            "eye": eye.tolist(),
            "target": target,
            "span_m": span,
        }
    )
    if "clip_m" in camera:
        scene.camera.data.clip_start, scene.camera.data.clip_end = camera["clip_m"]
    resolution = tuple(camera.get("resolution", (1920, 1080)))
    if resolution not in ((1920, 1080), (2560, 1440)):
        raise ValueError("explicit whole-crown diagnostic resolution required")
    scene.render.resolution_x, scene.render.resolution_y = resolution
    scene.eevee.taa_render_samples = 4
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private render output must be under/tmp")
    output.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str((output / f"{view}.png").resolve())
    ready = time.perf_counter()
    print(json.dumps({"stage": "render", "setup_s": ready - start}), flush=True)
    bpy.ops.render.render(write_still=True)
    report = {
        "left": presentation.get("recursive", "recursive"),
        "right": presentation.get("spatial", "spatial"),
        "view": view,
        "resolution": list(resolution),
        "eye": eye.tolist(),
        "target": target,
        "orthographic_span_m": span,
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
