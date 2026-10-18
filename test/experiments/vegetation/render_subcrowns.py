"""Matched full-source 2560x1440 subcrown architecture pictures."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import bpy
import gpu
import numpy as np
from crown_shoots import CrownShoots
from demand import Camera
from render_shoot_forms import environment
from shoot_form import ShootForm
from shoot_form_scene import leaf_objects, wood_objects
from spatial_group_source import load_source

CAMERAS = {
    "front": {"position": (0, -26, 10), "target": (0, 0, 9)},
    "oblique": {"position": (18.3847763, -18.3847763, 10), "target": (0, 0, 9)},
}


def render(inputs, output, view):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    camera = Camera(
        **CAMERAS[view], fov_y=50, width=2560, height=1440, near=0.05, far=1000
    )
    source = load_source(inputs)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    began = time.perf_counter()
    with (
        patch.object(
            CrownShoots, "group", side_effect=AssertionError("source poses in reader")
        ),
        patch.object(ShootForm, "build", side_effect=AssertionError("form rebuild")),
        patch("numpy.random.Generator", side_effect=AssertionError("RNG in reader")),
    ):
        form = ShootForm.load(inputs / "form.npz", expected_source=source.identity)
        leaf_stats = leaf_objects(source, form, camera, inputs, np.zeros(90000, bool))
        wood_stats = wood_objects(form, camera, inputs, near=False)
    scene = environment(camera)
    build_s = time.perf_counter() - began
    scene.render.filepath = str(output / "crown.png")
    print(json.dumps({"build_s": build_s, **leaf_stats, **wood_stats}), flush=True)
    began = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_identity": source.identity,
        "form_identity": form.identity,
        "view": view,
        "camera": CAMERAS[view],
        "fov_y_deg": 50,
        "resolution": [2560, 1440],
        "samples": 32,
        "renderer": gpu.platform.renderer_get(),
        "vendor": gpu.platform.vendor_get(),
        "build_s": build_s,
        "render_s": time.perf_counter() - began,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "png_sha256": hashlib.sha256((output / "crown.png").read_bytes()).hexdigest(),
        **leaf_stats,
        **wood_stats,
        "scope": "Original procedural study leaf material, generic smooth wood, C1 paths. Same whole-source 90000 blades and 8x1 geometry, new skeleton/poses. Shadows disabled; camera-only culling. No botanical, temporal, shadow, native or device admission.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--view", choices=CAMERAS, required=True)
    args = parser.parse_args()
    render(args.inputs, args.output, args.view)
