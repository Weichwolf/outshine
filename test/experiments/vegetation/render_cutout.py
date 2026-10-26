"""Same-light close comparison of saved original and authored-cutout leaf geometry."""

import argparse
import json
import time
from pathlib import Path

import bpy
from fern_material import palette
from outshine_ref.rendering.blender_camera import configure_camera
from render_biome_tiles import setup
from render_fern import model


def render(original, changed, output):
    start = time.perf_counter()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    materials = palette(original)
    left = model(original / "blade-0.npz", materials)
    right = model(changed / "blade-0.npz", materials)
    left.location = (-0.05, 0, 0.005)
    right.location = (0.05, 0, 0.005)
    eye, target = (0.12, -0.16, 0.32), (0, 0.055, 0.005)
    scene = setup("Laubwald", {"size_m": 0.5, "camera": eye, "target": target})
    scene.camera = configure_camera(
        {"projection": "orthographic", "eye": eye, "target": target, "span_m": 0.25}
    )
    scene.eevee.taa_render_samples = 16
    output.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str((output / "leaves.png").resolve())
    ready = time.perf_counter()
    print(json.dumps({"stage": "render", "setup_s": ready - start}), flush=True)
    bpy.ops.render.render(write_still=True)
    (output / "leaves.json").write_text(
        json.dumps(
            {
                "left": "unchanged stored Blade 0",
                "right": "new stored BladeCutout 0",
                "resolution": [1920, 1080],
                "samples": 16,
                "setup_s": ready - start,
                "render_s": time.perf_counter() - ready,
                "scope": "Same light and procedural material; only geometry changes. No species/material calibration.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--changed", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    render(args.original, args.changed, args.output)
