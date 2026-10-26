"""Render a fixed window-control scene with the same daylight or opposite sun side."""

import argparse
import math
import os
import sys
from pathlib import Path

import blender_city


def directional_light(scene):
    suns = [
        obj for obj in scene.objects if obj.type == "LIGHT" and obj.data.type == "SUN"
    ]
    if len(suns) != 1:
        raise ValueError("Expected one original directional light")
    return suns[0]


def alternate_studio(original):
    def studio(scene, data, samples, ground_span):
        result = original(scene, data, samples, ground_span)
        sun = directional_light(scene)
        sun.rotation_euler.z += math.pi
        return result | {
            "profile": "research-daylight-opposite-side-v1",
            "sun_rotation_radians": list(sun.rotation_euler),
        }

    return studio


def no_shadow_studio(original):
    def studio(scene, data, samples, ground_span):
        result = original(scene, data, samples, ground_span)
        sun = directional_light(scene)
        sun.data.use_shadow = False
        return result | {
            "profile": result["profile"] + "-no-sun-shadow-diagnostic",
            "sun_shadows": bool(sun.data.use_shadow),
        }

    return studio


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--geometry", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--opposite-sun", action="store_true")
    parser.add_argument("--no-sun-shadows", action="store_true")
    args = parser.parse_args()
    if args.opposite_sun:
        blender_city.studio = alternate_studio(blender_city.studio)
    if args.no_sun_shadows:
        blender_city.studio = no_shadow_studio(blender_city.studio)
    blender_city.render(args.geometry, args.output, 16)
    # Existing bpy teardown workaround, only after renderer outputs are flushed.
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)
