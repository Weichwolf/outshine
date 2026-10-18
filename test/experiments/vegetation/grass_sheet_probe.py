"""Isolate Blender sheet R/T and self-shadowing before interpreting whole grass."""

import argparse
import json
import os
import sys
from pathlib import Path

import bpy
import numpy as np
from grass_light_preview import mesh_object, setup, sheet
from mathutils import Vector
from PIL import Image
from preview_shadows import configure_preview


def run(output):
    if output.exists():
        raise ValueError("fresh isolated sheet probe directory required")
    output.mkdir(parents=True)
    view = np.array((np.sqrt(3) / 2, 0, 0.5))
    reflection, transmission = (
        np.array((0.08, 0.22, 0.025)),
        np.array((0.035, 0.12, 0.018)),
    )
    irradiance = np.array((3, 2.7, 2.4))
    rows = []
    for name, sign in (("front", 1), ("back", -1)):
        for shadows in (False, True):
            scene = setup(view)
            patches = [
                (reflection, transmission),
                (np.zeros(3), np.full(3, 0.25)),
                (np.zeros(3), np.full(3, 0.5)),
                (np.zeros(3), np.ones(3)),
            ]
            for i, (r, t) in enumerate(patches):
                y = (i - 1.5) * 0.28
                v = np.array(
                    (
                        (0, y - 0.125, -0.5),
                        (0, y + 0.125, -0.5),
                        (0, y + 0.125, 0.5),
                        (0, y - 0.125, 0.5),
                    )
                )
                mesh_object(
                    f"thin leaf {i}",
                    v,
                    np.array(((0, 1, 2), (0, 2, 3))),
                    sheet(f"R plus T {i}", r, t),
                )
            d = np.array((sign * np.sqrt(3) / 2, 0, 0.5))
            bpy.ops.object.light_add(type="SUN")
            lamp = bpy.context.object
            lamp.rotation_euler = (-Vector(d)).to_track_quat("-Z", "Y").to_euler()
            lamp.data.energy, lamp.data.color = 3, tuple(irradiance / 3)
            lamp.data.angle, lamp.data.use_shadow = 0, shadows
            configure_preview(scene)
            scene.eevee.taa_render_samples = 8
            target = output / f"{name}-shadow-{int(shadows)}.png"
            scene.render.filepath = str(target)
            bpy.ops.render.render(write_still=True)
            image = np.array(Image.open(target).convert("RGB"), float) / 255
            for i, (r, t) in enumerate(patches):
                x = int(960 + (i - 1.5) * 0.28 * 1920 / 1.4)
                q = image[480:600, x - 40 : x + 40]
                linear = np.where(q <= 0.04045, q / 12.92, ((q + 0.055) / 1.055) ** 2.4)
                expected = (r if sign > 0 else t) * abs(d[0]) * irradiance / np.pi
                row = {
                    "patch": i,
                    "direction": name,
                    "shadows": shadows,
                    "thickness_mode": bpy.data.materials[
                        f"R plus T {i}"
                    ].thickness_mode,
                    "measured_linear_rgb": linear.mean((0, 1)).tolist(),
                    "expected_direct_rgb": expected.tolist(),
                    "maximum_absolute_rgb_error": float(
                        abs(linear.mean((0, 1)) - expected).max()
                    ),
                }
                rows.append(row)
                print(json.dumps(row), flush=True)
    (output / "probe.json").write_text(json.dumps(rows, indent=2))
    if max(r["maximum_absolute_rgb_error"] for r in rows) > 0.005:
        raise ValueError("preset0.005 linear-RGB single-sheet error criterion failed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.output)
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)
