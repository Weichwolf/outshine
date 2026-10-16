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
            v = np.array(
                ((0, -0.5, -0.5), (0, 0.5, -0.5), (0, 0.5, 0.5), (0, -0.5, 0.5))
            )
            mesh_object(
                "single vertical thin leaf",
                v,
                np.array(((0, 1, 2), (0, 2, 3))),
                sheet("R plus T", reflection, transmission),
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
            q = (
                np.array(Image.open(target).convert("RGB"), float)[480:600, 900:1020]
                / 255
            )
            linear = np.where(q <= 0.04045, q / 12.92, ((q + 0.055) / 1.055) ** 2.4)
            expected = (
                (reflection if sign > 0 else transmission)
                * abs(d[0])
                * irradiance
                / np.pi
            )
            row = {
                "direction": name,
                "shadows": shadows,
                "measured_linear_rgb": linear.mean((0, 1)).tolist(),
                "expected_direct_rgb": expected.tolist(),
            }
            rows.append(row)
            print(json.dumps(row), flush=True)
    (output / "probe.json").write_text(json.dumps(rows, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.output)
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)
