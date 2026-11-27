"""Render the current Blender scene on the device and time the image write."""

import argparse
import json
import sys
from pathlib import Path
from time import perf_counter


def configure_gpu(scene, engine):
    import bpy
    import gpu

    gpu.init()
    backend = gpu.platform.backend_type_get()
    renderer = gpu.platform.renderer_get()
    if sys.platform == "darwin" and backend != "METAL":
        raise RuntimeError(f"Expected Metal for the reference render, got {backend}")
    if engine == "eevee":
        scene.render.engine = "BLENDER_EEVEE"
        return {"backend": backend, "device": renderer}
    if engine != "cycles":
        raise ValueError(f"Unknown reference renderer {engine}")
    preferences = bpy.context.preferences.addons["cycles"].preferences
    if backend == "METAL":
        preferences.compute_device_type = "METAL"
    preferences.refresh_devices()
    selected = []
    for device in preferences.devices:
        device.use = (
            device.type == preferences.compute_device_type and device.type != "CPU"
        )
        if device.use:
            selected.append(device.name)
    if not selected:
        raise RuntimeError("Cycles has no configured GPU")
    scene.render.engine = "CYCLES"
    scene.cycles.device = "GPU"
    return {"backend": preferences.compute_device_type, "device": selected}


def render_png(scene, output):
    import bpy

    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(Path(output).resolve())
    start = perf_counter()
    result = bpy.ops.render.render(write_still=True, scene=scene.name)
    if "FINISHED" not in result:
        raise RuntimeError(f"Blender render returned {result}")
    return perf_counter() - start


if __name__ == "__main__":
    import bpy

    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", choices=("eevee", "cycles"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--repeat",
        action="store_true",
        help="measure a second render in the same process",
    )
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])
    scene = bpy.context.scene
    device = configure_gpu(scene, args.engine)
    for index in range(2 if args.repeat else 1):
        print(
            json.dumps(
                {
                    **device,
                    "engine": args.engine,
                    "render": index + 1,
                    "seconds_to_png": render_png(scene, args.output),
                }
            ),
            flush=True,
        )
