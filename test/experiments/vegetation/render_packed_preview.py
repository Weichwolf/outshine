"""Bounded optical diagnosis of a complete packed Blender Place scene."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import bpy
import gpu


def render(scene_path, receipt_path, profile_path, output):
    start = time.perf_counter()
    receipt = json.loads(receipt_path.read_text())
    profile = json.loads(profile_path.read_text())
    if profile["purpose"] != "private-optical-diagnostic":
        raise ValueError("this entry point does not admit a presentation/cost gate")
    encoded = scene_path.read_bytes()
    scene_sha = hashlib.sha256(encoded).hexdigest()
    bpy.ops.wm.open_mainfile(filepath=str(scene_path.resolve()))
    scene = bpy.context.scene
    resolution = (scene.render.resolution_x, scene.render.resolution_y)
    if (
        list(resolution) != receipt["resolution"]
        or list(resolution) != profile["resolution"]
    ):
        raise ValueError("packed actual-camera resolution must remain unchanged")
    images = [image for image in bpy.data.images if image.source == "FILE"]
    if not images or any(image.packed_file is None for image in images):
        raise ValueError("complete packed procedural image assets required")
    samples_before = scene.eevee.taa_render_samples
    if (
        type(profile["samples"]) is not int
        or not 1 <= profile["samples"] <= samples_before
    ):
        raise ValueError("explicit limited diagnostic sample count required")
    scene.eevee.taa_render_samples = profile["samples"]
    output.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str((output / f"{receipt['place']}.png").resolve())
    began = time.perf_counter()
    print(
        json.dumps(
            {
                "stage": "render",
                "resolution": resolution,
                "stored_samples": samples_before,
                "diagnostic_samples": profile["samples"],
            }
        ),
        flush=True,
    )
    bpy.ops.render.render(write_still=True)
    image_path = Path(scene.render.filepath)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "scene_sha256": scene_sha,
        "scene_receipt_sha256": hashlib.sha256(receipt_path.read_bytes()).hexdigest(),
        "source_generator_commit": receipt["commit"],
        "place": receipt["place"],
        "scenario_sha256": receipt["scenario_sha256"],
        "resolution": resolution,
        "samples": profile["samples"],
        "stored_samples": samples_before,
        "camera_matrix": [list(row) for row in scene.camera.matrix_world],
        "renderer": gpu.platform.renderer_get(),
        "backend": gpu.platform.backend_type_get(),
        "render_s": time.perf_counter() - began,
        "elapsed_s": time.perf_counter() - start,
        "image_sha256": hashlib.sha256(image_path.read_bytes()).hexdigest(),
        "scope": "Private optical diagnostic only. Same complete packed scene and camera; lower sample count. No original64-sample cost pass, public Place image or quality acceptance.",
    }
    (output / f"{receipt['place']}-diagnostic.json").write_text(
        json.dumps(result, indent=2) + "\n"
    )
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--scene", type=Path, required=True)
    p.add_argument("--receipt", type=Path, required=True)
    p.add_argument("--profile", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    render(a.scene, a.receipt, a.profile, a.output)
