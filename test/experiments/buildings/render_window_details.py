"""Render a fixed window-control scene with the same daylight or opposite sun side."""

import argparse
import math
import os
import sys
from pathlib import Path

import blender_city


def alternate_studio(original):
    def studio(scene, data, samples, ground_span):
        result = original(scene, data, samples, ground_span)
        suns = [
            obj
            for obj in scene.objects
            if obj.type == "LIGHT" and obj.data.type == "SUN"
        ]
        if len(suns) != 1:
            raise ValueError("Expected one original directional light")
        sun = suns[0]
        sun.rotation_euler.z += math.pi
        return result | {
            "profile": "research-daylight-opposite-side-v1",
            "sun_rotation_radians": list(sun.rotation_euler),
        }

    return studio


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--geometry", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--opposite-sun", action="store_true")
    args = parser.parse_args()
    if args.opposite_sun:
        blender_city.studio = alternate_studio(blender_city.studio)
    blender_city.render(args.geometry, args.output, 16)
    # Existing bpy teardown workaround, only after renderer outputs are flushed.
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)
