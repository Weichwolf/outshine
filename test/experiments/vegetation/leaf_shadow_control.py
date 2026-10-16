"""Positive opaque-shadow control independent of the EEVEE transmission closure."""

import argparse
import hashlib
import json
import os
import sys
import time
from pathlib import Path

import bpy
import numpy as np
from grass_light_preview import camera_depth_receipt, mesh_object, setup, sheet
from leaf_stack_cases import IRRADIANCE, REFLECTION, VIEW, reflection_control_geometry
from mathutils import Vector
from PIL import Image
from preview_shadows import configure_preview


def run(output, clip_end):
    if output.exists():
        raise ValueError("fresh opaque-shadow control directory required")
    output.mkdir(parents=True)
    scene = setup(VIEW)
    scene.camera.data.clip_end = clip_end
    mat = sheet("ordinary opaque Lambert receiver", REFLECTION)
    receivers, occluder = reflection_control_geometry()
    for i, (v, f) in enumerate(receivers):
        mesh_object(f"receiver {i}", v, f, mat)
    mesh_object("opaque blocker outside camera centre rays", *occluder, mat)
    light = np.array((np.sqrt(3) / 2, 0, -0.5))
    bpy.ops.object.light_add(type="SUN")
    lamp = bpy.context.object
    lamp.rotation_euler = (-Vector(light)).to_track_quat("-Z", "Y").to_euler()
    lamp.data.energy, lamp.data.color = 3, tuple(IRRADIANCE / 3)
    lamp.data.angle = 0
    configure_preview(scene)
    scene.eevee.taa_render_samples = 8
    expected = REFLECTION * abs(light[0]) * IRRADIANCE / np.pi
    rows = []
    for shadows in (False, True):
        lamp.data.use_shadow = shadows
        target = output / f"opaque-shadow-{int(shadows)}.png"
        scene.render.filepath = str(target)
        began = time.perf_counter()
        bpy.ops.render.render(write_still=True)
        image = np.array(Image.open(target).convert("RGB"), float) / 255
        means = []
        for y in (-0.25, 0.25):
            x = int(960 + y * 1920 / 1.4)
            q = image[536:544, x - 16 : x + 16]
            linear = np.where(q <= 0.04045, q / 12.92, ((q + 0.055) / 1.055) ** 2.4)
            means.append(linear.mean((0, 1)))
        desired = np.array([expected, np.zeros(3) if shadows else expected])
        rows.append(
            {
                "shadows": shadows,
                "clip_end_m": clip_end,
                "camera": camera_depth_receipt(scene),
                "measured_rgb": np.array(means).tolist(),
                "expected_rgb": desired.tolist(),
                "max_error": float(abs(np.array(means) - desired).max()),
                "wall_s": time.perf_counter() - began,
                "image": target.name,
                "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
            }
        )
    (output / "result.json").write_text(json.dumps(rows, indent=2))
    if max(row["max_error"] for row in rows) > 0.005:
        raise ValueError("existing0.005 RGB Lambert criterion failed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--clip-end", type=float, default=1000)
    args = parser.parse_args()
    run(args.output, args.clip_end)
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)
