"""Cached actual source-support bodies; diagnostic mass geometry, not finished forests."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from blender_preview import material
from fern_material import attach_normals, source_normal
from outshine_ref.rendering.blender_mesh import mesh_object
from render_biome_tiles import setup


def render(inputs, output):
    began = time.perf_counter()
    summary = json.loads((inputs / "summary.json").read_text())
    with np.load(inputs / "coarse-support.npz", allow_pickle=False) as data:
        mesh = {k: data[k] for k in data.files}
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mat = material("habitat support mass", (0.035, 0.13, 0.02), 0.85)
    mat.node_tree.links.new(
        source_normal(mat), mat.node_tree.nodes["Principled BSDF"].inputs["Normal"]
    )
    obj = mesh_object(
        "cached actual source habitat",
        mesh["vertices"],
        mesh["faces"],
        mesh["normals"],
        mat,
        corner_normals=True,
    )
    attach_normals(obj, mesh["normals"])
    lower = mesh["vertices"][:, :2].min(axis=0)
    upper = mesh["vertices"][:, :2].max(axis=0)
    centre = (lower + upper) / 2
    span = float((upper - lower).max())
    obj.location = (-centre[0], -centre[1], 0)
    size = max(32.0, span * 1.2)
    scene = setup(
        "Laubwald",
        {
            "size_m": size,
            "camera": [size * 0.9, -size * 1.3, size * 0.85],
            "target": [0, 0, 0],
        },
    )
    scene.eevee.taa_render_samples = 8
    output.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str((output / "support.png").resolve())
    ready = time.perf_counter()
    print(json.dumps({"stage": "render", "setup_s": ready - began}), flush=True)
    bpy.ops.render.render(write_still=True)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "resolution": [1920, 1080],
        "renderer": gpu.platform.renderer_get(),
        "setup_s": ready - began,
        "render_s": time.perf_counter() - ready,
        "source_id": summary["source_id"],
        "input_sha256": hashlib.sha256(
            (inputs / "coarse-support.npz").read_bytes()
        ).hexdigest(),
        "presentation_translation_xy_m": (-centre).tolist(),
        "scope": summary["scope"],
    }
    (output / "support.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    render(args.inputs, args.output)
