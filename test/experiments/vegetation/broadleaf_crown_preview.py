"""Blender pair: stacked leaves on the left, matched-area terminal shoots right."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from blender_preview import mesh_object
from render_biome_tiles import materials, setup


def render(inputs, output, form, target):
    began = time.perf_counter()
    comparison = json.loads((inputs / "comparison.json").read_text())
    case = next(
        v
        for v in comparison["cases"]
        if v["form"] == form and v["target_area_m2"] == target
    )
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mats = materials("Laubwald")
    products = {}
    for x, label in ((-3.5, "density"), (3.5, "terminal")):
        path = inputs / f"{form}-{target}-{label}.npz"
        with np.load(path) as stored:
            mesh = {key: stored[key] for key in stored.files}
        obj = mesh_object(
            label,
            mesh["vertices"],
            mesh["faces"],
            mesh["normals"],
            mats[0],
            corner_normals=True,
        )
        for mat in mats[1:]:
            obj.data.materials.append(mat)
        obj.data.polygons.foreach_set(
            "material_index", mesh["surface"].astype(np.int32)
        )
        obj.data.uv_layers.new(name="physical leaf coordinates").data.foreach_set(
            "uv", mesh["uv"].ravel()
        )
        obj.location.x = x
        products[label] = hashlib.sha256(path.read_bytes()).hexdigest()
    recipe = {"size_m": 17, "camera": [0, -18, 12], "target": [0, 0, 3.5]}
    scene = setup("Laubwald", recipe)
    scene.camera.data.ortho_scale = 22
    output.mkdir(parents=True, exist_ok=True)
    path = output / f"{form}-{target}-comparison.png"
    scene.render.filepath = str(path.resolve())
    prepared = time.perf_counter()
    print(
        json.dumps(
            {
                "stage": "render",
                "form": form,
                "target_m2": target,
                "setup_s": prepared - began,
            }
        ),
        flush=True,
    )
    bpy.ops.render.render(write_still=True)
    report = {
        "form": form,
        "target_area_m2": target,
        "case": case["models"],
        "left": "density",
        "right": "terminal",
        "camera": recipe,
        "orthographic_width_m": 22,
        "resolution": [1920, 1080],
        "samples": 4,
        "setup_s": prepared - began,
        "render_s": time.perf_counter() - prepared,
        "renderer": gpu.platform.renderer_get(),
        "product_sha256": products,
        "source_commit": comparison["commit"],
        "renderer_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "image_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "scope": "Private whole-plant comparison, identical orthographic direction/scale, materials, light and physical leaf budget within reported discrete count error. Not botanical, forest or native runtime acceptance.",
    }
    path.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps({"form": form, "area": target, "render_s": report["render_s"]}),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--form", choices=("oak", "beech"), default="oak")
    parser.add_argument("--area", choices=(18, 55), type=int, default=55)
    args = parser.parse_args()
    render(args.inputs, args.output, args.form, args.area)
